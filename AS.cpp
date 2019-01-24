#include "AS.h"
#define NUM_OF_AS 5
unordered_map<string, bool> global_as_search_list;
vector<AS*> global_as_list;
void initializeGlobalList(vector<AS*> as_list)
{
	for(int i=0; i < NUM_OF_AS; ++i)
	{
		global_as_search_list.insert(make_pair("AS"+to_string(i+1), false));
		global_as_list.push_back(as_list[i]);
	}
}
void initilizeGlobalASList()
{
	for(auto it = global_as_search_list.begin(); it != global_as_search_list.end(); ++it)
	{
		it->second = false;
	}
}

void connectAStoAS(AS* as_1, AS* as_2, string if_1, string if_2, Router* rt_1, Router* rt_2)
{
	ASBlock* as1_as2 = new ASBlock;
	ASBlock* as2_as1 = new ASBlock;

	as1_as2->border_router = rt_1;
	as1_as2->border_interface = rt_1->getInterface(if_1);
	as1_as2->as_next_hop_ip = rt_2->getInterfaceIp(if_2);
	as_1->m_border_router_map.insert(make_pair(as_2->getName(), as1_as2));

	as2_as1->border_router = rt_2;
	as2_as1->border_interface = rt_2->getInterface(if_2);
	as2_as1->as_next_hop_ip = rt_1->getInterfaceIp(if_1);
	as_2->m_border_router_map.insert(make_pair(as_1->getName(), as2_as1));
}

AS::AS(string as_name, bool as_host, bool as_active, vector<Router*> border_routers)
{
	m_name.assign(as_name);
	m_host = as_host;
	m_active = as_active;
	for(int i=0; i < border_routers.size(); ++i)
	{
		m_border_router_list.push_back(border_routers[i]);
	}
}

vector<uint32_t> AS::getReachableDestinations()
{
	vector<uint32_t> ret_vec;
	for(auto it = m_reachable_dest_ip_list.begin(); it != m_reachable_dest_ip_list.end(); ++it)
	{
		ret_vec.push_back(it->first);
	}
	return ret_vec;
}
/*
Router* AS::getAvailableRouter()
{
	for(int i=0; i < m_border_router_list.size(); ++i)
	{
		if(m_border_router_list[i]->anyAvailableInterface())
		{
			return m_border_router_list[i];
		}
	}
	return NULL;
}
*/
bool AS::hookUpRequest(AS* new_as)
{
	vector<uint32_t> dest_ip_list = new_as->getReachableDestinations();
	vector<uint32_t> new_ip_list;
	for(int i=0; i < dest_ip_list.size(); ++i)
	{
		unordered_map<uint32_t, bool>::iterator it = m_reachable_dest_ip_list.find(dest_ip_list[i]);
		if(it == m_reachable_dest_ip_list.end()) //It is a new destination
		{
			new_ip_list.push_back(dest_ip_list[i]);
		}
	}

	if(0 < new_ip_list.size()) //So it is willing to connect to the new AS
	{
		Router* router_1 = (m_border_router_map[new_as->getName()])->border_router;
		Router* router_2 = (new_as->m_border_router_map[m_name])->border_router;
		
		//Make this part as a helper function for multiple senders case later
		for(int i=0; i < new_ip_list.size(); ++i) //Announcement that there is a new destination.
		{
			//Update the routers
			for(int j=0; j < m_border_router_list.size(); ++j)
			{
				for(int k=0; k < m_border_router_list[j]->m_routing_table.size(); ++k)
				{
					if(m_border_router_list[j]->m_routing_table[k]->dest_addr == new_ip_list[i])
					{
						m_border_router_list[j]->m_routing_table[k]->in_use = true;	
					}
				}
			}

			//check whether a path already exists or not
			PathListBlock* new_path_list = new PathListBlock;
			pair<uint32_t, vector<string> > new_pair; //dest and path
			new_pair.first = new_ip_list[i];

			new_pair.second.push_back(new_as->getName());

			//Path insertion implementation needed......
			new_path_list->path = new_pair;
			new_path_list->used_path = true;
			m_path_list.push_back(new_path_list);
			m_reachable_dest_ip_list.insert(make_pair(new_ip_list[i], true)); //Update the Path List and the Reachable Destination Done

			initilizeGlobalASList(); 

			global_as_search_list[m_name] = true;
			global_as_search_list[new_as->getName()] = true;
			queue<pair<AS*, BGPAttr*> > as_queue;

			for(auto it = m_adjacent_as_list.begin(); it != m_adjacent_as_list.end(); ++it)
			{
				if(global_as_search_list[it->first] == false)
				{
					global_as_search_list[it->first] = true;
					BGPAttr* new_bgp_attr = new BGPAttr;

					new_bgp_attr->path.push_back(m_name);
					new_bgp_attr->path.push_back(new_as->getName());

					ASBlock* neighbor_as = m_border_router_map[it->first];
					new_bgp_attr->next_hop_ip_addr = neighbor_as->border_interface->ip;

					new_bgp_attr->dest_ip_addr = new_ip_list[i];

					as_queue.push(make_pair(it->second, new_bgp_attr));
				}
			}

			while(!as_queue.empty())
			{
				pair<AS*, BGPAttr*> elem = as_queue.front();
				as_queue.pop();
				elem.first->broadcastUpdate(as_queue, elem.second);
			}
			
		}
		return true;
	}
	return false;
}

void AS::broadcastUpdate(queue<pair<AS*, BGPAttr*> >& as_queue, BGPAttr* bgp_attr)
{
	if(m_host) //Host AS does not need to do anything
	{
		return;
	}
	unordered_map<uint32_t, bool>::iterator got = m_reachable_dest_ip_list.find(bgp_attr->dest_ip_addr);
	if(got == m_reachable_dest_ip_list.end()) //OK. I will update!
	{
		ASBlock* as_block = m_border_router_map[bgp_attr->path[0]];

		m_reachable_dest_ip_list.insert(make_pair(bgp_attr->dest_ip_addr, true));

		//Update other border routers here
		for(int i=0; i < m_border_router_list.size(); ++i)
		{
			for(int j=0; j < m_border_router_list[i]->m_routing_table.size(); ++j)
			{
				if(m_border_router_list[i]->m_routing_table[j]->dest_addr == bgp_attr->dest_ip_addr)
				{
					m_border_router_list[i]->m_routing_table[j]->in_use = true;	
				}
			}
		}		

		vector<string> new_path_for_me;
		
		for(int i=0; i < bgp_attr->path.size(); ++i)
		{
			new_path_for_me.push_back(bgp_attr->path[i]);
		}

		PathListBlock* new_path_list = new PathListBlock; //Path List Update
		pair<uint32_t, vector<string> > new_pair; //dest and path
		new_pair.first = bgp_attr->dest_ip_addr;
		new_pair.second = new_path_for_me;
		new_path_list->path = new_pair;
		new_path_list->used_path = true;
		m_path_list.push_back(new_path_list);

		for(auto it = m_adjacent_as_list.begin(); it != m_adjacent_as_list.end(); ++it)
		{
			if(global_as_search_list[it->first] == false)
			{
				global_as_search_list[it->first] = true;

				BGPAttr* new_bgp_attr = new BGPAttr;
				new_bgp_attr->path.push_back(m_name);
				for(int i=0; i < new_path_for_me.size(); ++i)
				{
					new_bgp_attr->path.push_back(new_path_for_me[i]);
				}
				ASBlock* neighbor_as = m_border_router_map[it->first];
				new_bgp_attr->next_hop_ip_addr = neighbor_as->border_interface->ip;
				new_bgp_attr->dest_ip_addr = bgp_attr->dest_ip_addr;

				as_queue.push(make_pair(it->second, new_bgp_attr));
			}
		}
	}
}

void AS::withdrawl()
{
	vector<uint32_t> dest_ip_list;
	for(auto it = m_reachable_dest_ip_list.begin(); it != m_reachable_dest_ip_list.end(); ++it)
	{
		dest_ip_list.push_back(it->first);
	}

	for(int i=0; i < global_as_list.size(); ++i)
	{
		global_as_list[i]->withdrawalSignal(m_name, dest_ip_list);
	}
}
//Lock should be added

void AS::flipBorderRouters(uint32_t dest_ip_addr)
{
	for(int i=0; i < m_border_router_list.size(); ++i)
	{
		m_border_router_list[i]->flipRoutingTable(dest_ip_addr);
	}
}

bool AS::contain(vector<string>& vec, string elem)
{
	for(int i=0; i < vec.size(); ++i)
	{
		if(vec[i].compare(elem) == 0)
		{
			return true;
		}
	}
	return false;
}

bool AS::isThereAlternative(uint32_t dest_ip_addr, string disabled_as)
{
	bool ret_val = false;
	for(int i=0; i < m_path_list.size(); ++i)
	{
		if(m_path_list[i]->path.first == dest_ip_addr)
		{
			if(!contain(m_path_list[i]->path.second, disabled_as))
			{
				if(!m_path_list[i]->used_path)
				{
					m_path_list[i]->used_path = true;
					ret_val = true; //It means there is something to do more
				}
			}
			else
			{
				m_path_list[i]->used_path = false;
			}
		}
	}
	return ret_val;
}

void AS::withdrawalSignal(string as_name, vector<uint32_t> unreachable_dest_ip_list)
{
	if(m_host)
	{
		return;
	}
	for(int i=0; i < unreachable_dest_ip_list.size(); ++i)
	{
		for(int j=0; j < m_border_router_list.size(); ++j)
		{
			for(int k=0; k < m_border_router_list[j]->m_routing_table.size(); ++k)
			{
				if(m_border_router_list[j]->m_routing_table[k]->dest_addr == unreachable_dest_ip_list[i])
				{
					m_border_router_list[j]->m_routing_table[k]->in_use = false;
				}	
			}
		}
		
		m_reachable_dest_ip_list.erase(unreachable_dest_ip_list[i]);
		
		for(int m=0; m < m_path_list.size(); ++m)
		{
			if(m_path_list[m]->path.first == unreachable_dest_ip_list[i])
			{
				m_path_list.erase(m_path_list.begin() + m);
				break;
			}
		}
	}
}






