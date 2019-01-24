#include "host.h"
#ifndef __AS_H__
#define __AS_H__

struct BGPAttr
{
	vector<string> path;
	uint32_t next_hop_ip_addr;
	uint32_t dest_ip_addr;
};

struct ASBlock
{
	Router* border_router; //The border router of this AS
	Interface* border_interface; //The interface of border_router connected to AS
	uint32_t as_next_hop_ip; //The IP address of the border router of the connected AS
};

struct PathListBlock
{
	pair<uint32_t, vector<string>> path; //first = destination IP, second = path
	bool used_path;
};

class AS
{
public:
	AS(){}
	AS(string as_name, bool as_host, bool as_active, vector<Router*> border_routers);
	void withdrawl();
	void withdrawalSignal(string as_name, vector<uint32_t> unreachable_dest_ip_list); //as_name: the name of AS that is going to be removed
	bool isThereAlternative(uint32_t dest_ip_addr, string disabled_as);
	bool contain(vector<string>& vec, string elem);
	vector<uint32_t> getReachableDestinations(); 
	Router* getAvailableRouter();
	bool hookUpRequest(AS* new_as);
	void broadcastUpdate(queue<pair<AS*, BGPAttr*> >& as_queue, BGPAttr* bgp_attr);
	void flipBorderRouters(uint32_t dest_ip_addr);
	string getName()
	{
		return m_name;
	}
	void addAdjacentAS(string as_name, AS* as)
	{
		m_adjacent_as_list.insert(make_pair(as_name, as));
	}
	void addPathList(uint32_t dest_ip, vector<string> path_list, bool used)
	{
		PathListBlock* new_path_block = new PathListBlock;
		pair<uint32_t, vector<string>> path_pair;
		for(int i=0; i < path_list.size(); ++i)
		{
			path_pair.second.push_back(path_list[i]);
		}
		path_pair.first = dest_ip;
		new_path_block->path = path_pair;
		new_path_block->used_path = used;
	}
	void addReachableDestination(uint32_t dest_ip)
	{
		m_reachable_dest_ip_list.insert(make_pair(dest_ip, true));
	}
public:
	unordered_map<string, ASBlock*> m_border_router_map;
private:
	string m_name; //Assume that this name is unique
	bool m_host; //Whether it is a host AS or not
	bool m_active; //Indicates whether this AS is active or not
	vector<PathListBlock*> m_path_list;
	unordered_map<string, AS*> m_adjacent_as_list; //key = the name of adjacent AS
	unordered_map<uint32_t, bool> m_reachable_dest_ip_list;
	vector<Router*> m_border_router_list;
};	
void initializeGlobalList(vector<AS*> as_list);
//unordered_map<string, vector<string> > global_as_path_list;
void initilizeGlobalASList();
void connectAStoAS(AS* as_1, AS* as_2, string if_1, string if_2, Router* rt_1, Router* rt_2);
#endif
