#include "router.h"
//The constructor for building the routing table and the interface list
Router::Router(ifstream& in_file, uint32_t r_id)
{
	m_id = r_id;
	string line;
	while(getline(in_file, line))
	{
		vector<string> spliced = splice(line, ' ');
		assert(spliced.size() == SPLICE_SIZE);

		RoutingTable* new_rt_elem = new RoutingTable;

		vector<string> ip_spliced_str = splice(spliced[0], '.');
		vector<uint32_t> ip_spliced_uint = fromStrToUintVector(ip_spliced_str);
		uint32_t dest_ip = convertTo32Bits(ip_spliced_uint);
		new_rt_elem->dest_addr = dest_ip;

		ip_spliced_str.clear();
		ip_spliced_uint.clear();

		ip_spliced_str = splice(spliced[1], '.');
		ip_spliced_uint = fromStrToUintVector(ip_spliced_str);
		uint32_t next_hop_ip = convertTo32Bits(ip_spliced_uint);
		new_rt_elem->next_hop = next_hop_ip;

		ip_spliced_str.clear();
		ip_spliced_uint.clear();

		ip_spliced_str = splice(spliced[2], '.');
		ip_spliced_uint = fromStrToUintVector(ip_spliced_str);
		uint32_t mask_ip = convertTo32Bits(ip_spliced_uint);
		new_rt_elem->mask = mask_ip;

		ip_spliced_str.clear();
		ip_spliced_uint.clear();

		new_rt_elem->interface_name.assign(spliced[3]);

		m_routing_table.push_back(new_rt_elem);

		Interface* new_if_elem = new Interface;

		new_if_elem->name.assign(spliced[3]);

		vector<string> mac_spliced_str = splice(spliced[4], ':');
		assert(mac_spliced_str.size() == MAC_SPLICE_SIZE);

		for(int i=0; i < mac_spliced_str.size(); ++i)
		{
			int mac_elem = stoi(mac_spliced_str[i], 0, 16);
			uint8_t char_elem = (uint8_t)mac_elem;
			new_if_elem->mac_addr[i] = char_elem;
		}

		ip_spliced_str = splice(spliced[5], '.');
		ip_spliced_uint = fromStrToUintVector(ip_spliced_str);
		uint32_t interface_ip = convertTo32Bits(ip_spliced_uint);
		new_if_elem->ip = interface_ip;

		m_interface_list.insert(make_pair(spliced[3], new_if_elem));

		queue<pair<uint32_t, uint8_t*> > new_input_queue;
		queue<pair<uint32_t, uint8_t*> > new_output_queue;

		m_input_queue.insert(make_pair(spliced[3], new_input_queue));
		m_output_queue.insert(make_pair(spliced[3], new_output_queue));

		if(stoi(spliced[6]) == 1)
		{
			new_rt_elem->in_use = true;
		}
		else
		{
			new_rt_elem->in_use = false;
		}
	}

	m_num_of_interfaces = m_interface_list.size();

	m_address_trie = new RouterTrie;
	m_address_trie->child_trie[0] = NULL;
	m_address_trie->child_trie[1] = NULL;
	m_address_trie->dest_addr = NULL;
	trieConstructor();

	m_avg_queue_length = AVG_Q_LEN;
	m_min_threshold = MIN_TH;
	m_max_threshold = MAX_TH;
}

void* sendThread(void* thread_arg)
{
	assert(thread_arg);
	SendThreadArg* send_arg = (SendThreadArg*)thread_arg;
	pthread_mutex_lock(send_arg->dst_queue_mutex);
	while(!send_arg->src_queue_ptr->empty())
	{
		pair<uint32_t, uint8_t*> elem = send_arg->src_queue_ptr->front();
		send_arg->src_queue_ptr->pop();
		send_arg->dst_queue_ptr->push(elem);
	}
	pthread_mutex_unlock(send_arg->dst_queue_mutex);
	if(send_arg->dst_queue_cv)
	{
		pthread_cond_signal(send_arg->dst_queue_cv);
	}
	return NULL;
}

Interface* Router::getInterfaceByNextHop(uint32_t next_hop_ip)
{
	for(int i=0; i < m_routing_table.size(); ++i)
	{
		if(m_routing_table[i]->next_hop == next_hop_ip)
		{
			return m_interface_list[m_routing_table[i]->interface_name];
		}
	}
	return NULL;
}

void Router::flipRoutingTable(uint32_t dest_ip_addr)
{
	for(int i=0; i < m_routing_table.size(); ++i)
	{
		if(m_routing_table[i]->dest_addr == dest_ip_addr)
		{
			if(m_routing_table[i]->in_use)
			{
				m_routing_table[i]->in_use = false;	
			}
			else
			{
				m_routing_table[i]->in_use = true;
			}
		}
	}
}

queue<pair<uint32_t, uint8_t*> >* Router::getOutputQueue(string interface)
{
	unordered_map<string, queue<pair<uint32_t, uint8_t*> > >::iterator it = m_output_queue.find(interface);
	return &(it->second);
}

void Router::addThreadArg(string interface, SendThreadArg* send_arg)
{
	m_send_thread_arg_list.insert(make_pair(interface, send_arg));
}

bool Router::sendToTheNextHop()
{
	pthread_t thread[m_num_of_interfaces]; //One thread for each m_output_queue
	uint32_t counter = 0;
	for(auto it = m_output_queue.begin(); it != m_output_queue.end(); ++it)
	{
		if(!it->second.empty())
		{	
			unordered_map<string, SendThreadArg*>::iterator got = m_send_thread_arg_list.find(it->first);
			if(got == m_send_thread_arg_list.end())
			{
				fprintf(stderr, "SendThreadArg Error \n");
			}
			pthread_create(&thread[counter], NULL, sendThread, (void*)got->second);
			++counter;
		}
	}

	assert(counter <= m_num_of_interfaces);

	for(uint32_t i=0; i < counter; ++i)
	{
		pthread_join(thread[i], NULL);
	}

	//Timer function should be added.
	return true;
}

bool Router::randomEarlyDetection()
{
	if(m_avg_queue_length < m_min_threshold)
	{
		return false;
	}

	if(m_max_threshold < m_avg_queue_length)
	{
		return true;
	}

	double prob = (m_avg_queue_length - m_min_threshold)*1.0 / (m_max_threshold - m_min_threshold)*1.0;
	uint32_t drop_prob = prob*100.0;
	srand(time(NULL));
	uint32_t rand_num = rand()%100 + 1;
	if(rand_num <= drop_prob)
	{
		return true;
	}
	return false;
}

void Router::sweepArpReqs()
{
	for(auto it = m_arpreq_times_list.begin(); it != m_arpreq_times_list.end(); ++it)
	{
		time_t curr_time = time(NULL);
		time_t elapsed_time = curr_time - (it->second->sent);
		if(4 < elapsed_time)
		{
			if(5 < it->second->times_sent)
			{
				m_arpreq_queue.erase(it->second->ip);
				m_arpreq_times_list.erase(it->second->ip);
			}
			else
			{
				//cout << "Sending Arp Request" << endl;
				Interface* out_interface = getInterface(it->second->iface);
				uint8_t* arp_request = createArpRequestHeader(out_interface, it->second->ip);
				addPacketToOutputQueue(arp_request, sizeof(EthernetHeader)+sizeof(ArpHeader), it->second->iface);
				++(it->second->times_sent);
				it->second->sent = time(NULL);
			}
		}
	}
}

//in_if: the interface pointer that an ARP request is going to go out
uint8_t* Router::createArpRequestHeader(Interface* in_if, uint32_t target_ip) // Ethernet Header + Arp Header both
{
    uint8_t* buf = (uint8_t*)malloc(sizeof(EthernetHeader) + sizeof(ArpHeader)); // Ethernet Header + Arp Header => buf

    EthernetHeader* ethernet_hdr = (EthernetHeader*)buf;
    memcpy(ethernet_hdr->ether_shost, in_if->mac_addr, ETHER_ADDR_LEN);
    
    for(int i=0; i < ETHER_ADDR_LEN; ++i)
    {
        ethernet_hdr->ether_dhost[i] = 0xFF;
    }
    ethernet_hdr->ether_type = ethertype_arp;

    ArpHeader* arp_hdr = createArpHeader(in_if, target_ip);

    memcpy(buf + sizeof(EthernetHeader), arp_hdr, sizeof(ArpHeader));

    free(arp_hdr);

    return buf;
}

ArpHeader* Router::createArpHeader(Interface* in_if, uint32_t target_ip)
{
    ArpHeader* arp_hdr = (ArpHeader*)malloc(sizeof(ArpHeader));

    arp_hdr->ar_hln = ETHER_ADDR_LEN;
    arp_hdr->ar_op = arp_op_request;

    memcpy(arp_hdr->ar_sha, in_if->mac_addr, ETHER_ADDR_LEN);
    arp_hdr->ar_sip = in_if->ip;

    char* zero_fill = (char*)malloc(ETHER_ADDR_LEN);
    for(int i=0; i < ETHER_ADDR_LEN; ++i)
    {
        zero_fill[i] = 0;
    }
    memcpy(arp_hdr->ar_tha, zero_fill, ETHER_ADDR_LEN);
    arp_hdr->ar_tip = target_ip; 
    arp_hdr->ar_hrd = arp_hrd_ethernet;
    arp_hdr->ar_pln = 4;
    arp_hdr->ar_pro = 2048;
    free(zero_fill);
    return arp_hdr;
}
uint8_t* Router::echoIcmp(uint8_t* buf)
{
    EthernetHeader* ethernet_ptr = (EthernetHeader*)buf;
    uint8_t* temp_mac = (uint8_t*)malloc(ETHER_ADDR_LEN);
    memcpy(temp_mac, ethernet_ptr->ether_dhost, ETHER_ADDR_LEN);
    memcpy(ethernet_ptr->ether_dhost, ethernet_ptr->ether_shost, ETHER_ADDR_LEN);
    memcpy(ethernet_ptr->ether_shost, temp_mac, ETHER_ADDR_LEN);

    IpHeader* ip_ptr = (IpHeader*)(buf + sizeof(EthernetHeader));
    IcmpT11Header* icmp_hdr = (IcmpT11Header*)(buf + sizeof(EthernetHeader) + sizeof(IpHeader));
    uint32_t temp_ip = ip_ptr->ip_src;
    ip_ptr->ip_src = ip_ptr->ip_dst;
    ip_ptr->ip_dst = temp_ip;
    ip_ptr->ip_sum = 0;
    ip_ptr->ip_sum = cksum(ip_ptr, sizeof(IpHeader));

    icmp_hdr->icmp_type = 0;
    icmp_hdr->icmp_sum = 0;
    icmp_hdr->icmp_sum = cksum(icmp_hdr, sizeof(IcmpT11Header));
    free(temp_mac);
    return buf;
}

void Router::inputToOutputQueue() // Needs to be locked
{
	 for(auto it = m_input_queue.begin(); it != m_input_queue.end(); ++it)
     {
        if(!it->second.empty())
        {
        	while(!it->second.empty())
        	{
        		uint8_t* packet = (it->second.front()).second;
        		uint32_t len = (it->second.front()).first;
        		it->second.pop();
        		handlePacket(packet, len, it->first);
        	}
        }
     }
}

bool Router::isThisIpForMe(uint8_t* packet)
{
    assert(packet);
    IpHeader* ip_ptr = (IpHeader*)(packet + sizeof(EthernetHeader));
    
    for(auto it = m_interface_list.begin(); it != m_interface_list.end(); ++it)
    {
    	if(it->second->ip == ip_ptr->ip_dst)
    	{
    		return true;
    	}
    }
  
    return false;
}

bool Router::isThisArpForMe(uint32_t target_ip)
{
	for(auto it = m_interface_list.begin(); it != m_interface_list.end(); ++it)
	{
		if(it->second->ip == target_ip)
		{
			return true;
		}
	}
	return false;
}

void Router::trieConstructor()
{
    //printf("constructing trie \n");
    for(int i=0; i < m_routing_table.size(); ++i)
    {
    	trieInsert(m_routing_table[i]);
    }
}

RouterTrie* Router::trieNodeConstructor()
{
    RouterTrie* newNode = (RouterTrie*)malloc(sizeof(RouterTrie));
    newNode->child_trie[0] = NULL;
    newNode->child_trie[1] = NULL;
    newNode->dest_addr = NULL;
    return newNode;
}

void Router::trieInsert(RoutingTable* rtPtr) 
{
    unsigned char counter = 0;
    uint32_t mask = 0x80000000;
    uint32_t tempRtMask = rtPtr->mask;
    RouterTrie* trieWalker = m_address_trie;
    while((tempRtMask & mask) != 0)
    {
        tempRtMask = tempRtMask << 1;
        ++counter;
    }
    uint32_t maskedAddr = rtPtr->mask & rtPtr->dest_addr;
    int i = 0;
    for(;i < counter; ++i)
    {
        if((maskedAddr & mask) != 0) // 1
        {
            if(trieWalker->child_trie[1] == NULL)
            {
                trieWalker->child_trie[1] = trieNodeConstructor();
            }
            trieWalker = trieWalker->child_trie[1];
        }
        else
        {
            if(trieWalker->child_trie[0] == NULL)
            {
                trieWalker->child_trie[0] = trieNodeConstructor();
            }
            trieWalker = trieWalker->child_trie[0];
        }
        maskedAddr = maskedAddr << 1;
    }
    if(trieWalker->dest_addr != NULL)
    {
    	if(trieWalker->dest_addr->in_use == true)
    	{
    		return;
    	}
    }
    trieWalker->dest_addr = rtPtr;
}

RoutingTable* Router::longestPrefixMatch(uint32_t dest_ip_addr) // destIpAddr Host Byte Ordered
{
    uint32_t bit_mask = 0x80000000;
    RoutingTable* recent_match = NULL;
    RouterTrie* trie_walker = m_address_trie;
    
    for(int i=0; i < 32; ++i) 
    {
        if((bit_mask & dest_ip_addr) != 0) // 1
        {
            if(trie_walker->child_trie[1] == NULL)
            {
               return recent_match;
            }
            trie_walker = trie_walker->child_trie[1];
        }
        else
        {
            if(trie_walker->child_trie[0] == NULL)
            {
               return recent_match;
            }
            trie_walker = trie_walker->child_trie[0];
        }
        if(trie_walker->dest_addr != NULL)
        {
            recent_match = trie_walker->dest_addr;
        }
        dest_ip_addr = dest_ip_addr << 1;
    }
    if(recent_match != NULL)
    {
    	if(recent_match->in_use == false)
    	{
    		return NULL;
    	}
    }
    return recent_match;
}

IcmpT11Header* Router::createIcmpHeader(uint8_t type, uint8_t code, IpHeader* old_ip_hdr)
{
    assert(old_ip_hdr);
    IcmpT11Header* new_icmp_hdr = (IcmpT11Header*)malloc(sizeof(IcmpT11Header));
    new_icmp_hdr->icmp_type = type;
    new_icmp_hdr->icmp_code = code;
    if(type == icmp_dest_unreachable)
    {
    	new_icmp_hdr->unused = old_ip_hdr->ip_dst;
    }
    else
    {
    	new_icmp_hdr->unused = 0;
    }
    if(old_ip_hdr != NULL)
    {
        memcpy(new_icmp_hdr->data, old_ip_hdr, 28);
    }
    new_icmp_hdr->icmp_sum = 0;
    new_icmp_hdr->icmp_sum = cksum(new_icmp_hdr, sizeof(IcmpT11Header)); 
    return new_icmp_hdr;
}

uint8_t* Router::createIcmpResponse(uint8_t* buf, IcmpT11Header* icmp_hdr, string iface)
{
    assert(icmp_hdr);

    uint8_t* new_buf = (uint8_t*)malloc(sizeof(EthernetHeader) + sizeof(IpHeader) + sizeof(IcmpT11Header));

    EthernetHeader* ethernet_hdr = (EthernetHeader*)buf;
    IpHeader* ip_hdr = (IpHeader*)(buf + sizeof(EthernetHeader));

    EthernetHeader* new_ethernet_hdr = (EthernetHeader*)new_buf;
    IpHeader* new_ip_hdr = (IpHeader*)(new_buf + sizeof(EthernetHeader));
    IcmpT11Header* new_icmp_hdr = (IcmpT11Header*)(new_buf + sizeof(EthernetHeader) + sizeof(IpHeader));

    memcpy(new_ethernet_hdr, ethernet_hdr, sizeof(EthernetHeader));
    memcpy(new_ip_hdr, ip_hdr, sizeof(IpHeader));
    memcpy(new_icmp_hdr, icmp_hdr, sizeof(IcmpT11Header));

    unordered_map<string, Interface*>::iterator it = m_interface_list.find(iface);

    memcpy(new_ethernet_hdr->ether_shost, it->second->mac_addr, ETHER_ADDR_LEN);
    memcpy(new_ethernet_hdr->ether_dhost, ethernet_hdr->ether_shost, ETHER_ADDR_LEN);
    
    new_ip_hdr->ip_src = it->second->ip;
  	new_ip_hdr->ip_dst = ip_hdr->ip_src;
    new_ip_hdr->ip_ttl = 64;
    new_ip_hdr->ip_len = sizeof(IpHeader) + sizeof(IcmpT11Header);
    new_ip_hdr->ip_p = ip_protocol_icmp;
    new_ip_hdr->ip_sum = 0;
    new_ip_hdr->ip_sum = cksum(new_ip_hdr, sizeof(IpHeader));
 
    return new_buf;
}

//interface: the outgoing interface
void Router::addPacketToOutputQueue(uint8_t* buf, uint32_t len, string interface)
{
	unordered_map<string, queue<pair<uint32_t, uint8_t*>>>::iterator it = m_output_queue.find(interface);
	if(it == m_output_queue.end())
	{
		fprintf(stderr, "Output Queue Error \n");
        return;
	}
	it->second.push(make_pair(len, buf));
}

ArpCacheEntry* Router::arpCacheLookUp(uint32_t next_hop_ip)
{
	unordered_map<uint32_t, ArpCacheEntry*>::iterator it = m_arp_cache.find(next_hop_ip);
	if(it == m_arp_cache.end())
	{
		return NULL;
	}
	return it->second;
}

Interface* Router::getInterface(string interface)
{
	unordered_map<string, Interface*>::iterator it = m_interface_list.find(interface);
	assert(it != m_interface_list.end());
	if(it == m_interface_list.end())
	{
		return NULL;
	}
	return it->second;
}

uint32_t Router::getInterfaceIp(string interface)
{
	unordered_map<string, Interface*>::iterator it = m_interface_list.find(interface);
	assert(it != m_interface_list.end());
	if(it == m_interface_list.end())
	{
		return 0;
	}
	return it->second->ip;
}

//req_ip: the IP address that an ARP request is going to be sent
//interface: the name of the interface of the current router that an ARP request is going to be sent
void Router::arpCacheQueueReq(uint32_t req_ip, uint8_t* packet, uint32_t packet_len, string interface)
{
	ReqPacket* new_arp_req = new ReqPacket;
	new_arp_req->buf = packet;
	new_arp_req->len = packet_len;
	(new_arp_req->iface).assign(interface);

	unordered_map<uint32_t, queue<ReqPacket*> >::iterator it = m_arpreq_queue.find(req_ip);
	if(it == m_arpreq_queue.end())
	{
		//printf("I am here! \n");
		//cout << "Arp Cache: " << req_ip << endl;
		queue<ReqPacket*> new_queue;
		new_queue.push(new_arp_req);
		m_arpreq_queue.insert(make_pair(req_ip, new_queue));
		ArpReq* arp_time = new ArpReq;

		arp_time->ip = req_ip;
		arp_time->times_sent = 0;
		arp_time->iface.assign(interface);

		m_arpreq_times_list.insert(make_pair(req_ip, arp_time));
	}
	else
	{
		it->second.push(new_arp_req);
	}
}

void Router::handleArpRequestToMe(uint8_t* buf, Interface* if_ptr)
{
    EthernetHeader* ethernet_hdr = (EthernetHeader*)buf;
    ArpHeader* arp_hdr = (ArpHeader*)(buf + sizeof(EthernetHeader));

    memcpy(ethernet_hdr->ether_dhost, ethernet_hdr->ether_shost, ETHER_ADDR_LEN);
    memcpy(ethernet_hdr->ether_shost, if_ptr->mac_addr, ETHER_ADDR_LEN);

    memcpy(arp_hdr->ar_tha, arp_hdr->ar_sha, ETHER_ADDR_LEN);
    memcpy(arp_hdr->ar_sha, if_ptr->mac_addr, ETHER_ADDR_LEN);

    arp_hdr->ar_tip = arp_hdr->ar_sip;
    arp_hdr->ar_sip = if_ptr->ip;

    arp_hdr->ar_op = arp_op_reply;  
}
 
void Router::handleArpReplyToMe(uint8_t* packet)
{
	EthernetHeader* ethernet_hdr = (EthernetHeader*)packet;
	ArpHeader* arp_hdr = (ArpHeader*)(packet + sizeof(EthernetHeader));
	ArpCacheEntry* new_entry = (ArpCacheEntry*)malloc(sizeof(ArpCacheEntry));
	memcpy(new_entry->mac, arp_hdr->ar_sha, ETHER_ADDR_LEN);
	new_entry->ip = arp_hdr->ar_sip;
	new_entry->added = time(NULL);
	m_arp_cache.insert(make_pair(arp_hdr->ar_sip, new_entry));

	unordered_map<uint32_t, queue<ReqPacket*> >::iterator it = m_arpreq_queue.find(arp_hdr->ar_sip);

	assert(it != m_arpreq_queue.end());

	while(!it->second.empty())
	{
		ReqPacket* elem = it->second.front();
		it->second.pop();
		EthernetHeader* in_ethernet_hdr = (EthernetHeader*)elem->buf;
		IpHeader* in_ip_hdr = (IpHeader*)(elem->buf + sizeof(EthernetHeader));

		memcpy(in_ethernet_hdr->ether_shost, arp_hdr->ar_tha, ETHER_ADDR_LEN);
		memcpy(in_ethernet_hdr->ether_dhost, arp_hdr->ar_sha, ETHER_ADDR_LEN);

		--(in_ip_hdr->ip_ttl);
		in_ip_hdr->ip_sum = 0;
		in_ip_hdr->ip_sum = cksum(in_ip_hdr, sizeof(IpHeader));

		addPacketToOutputQueue(elem->buf, elem->len, elem->iface);
	}

	m_arpreq_queue.erase(arp_hdr->ar_sip);
	m_arpreq_times_list.erase(arp_hdr->ar_sip);
}

void Router::arpCacheCleaning()
{
	for(auto it = m_arp_cache.begin(); it != m_arp_cache.end(); ++it)
	{
		time_t current_time = time(NULL);
		if(ARP_CACHE_EXPIRED < current_time - it->second->added)
		{
			m_arp_cache.erase(it->first);
		}
	}
}

//packet: the content
//interface: the name of interface that a pakcet came in
//packet_len: the length of the packet
void Router::handlePacket(uint8_t* packet, uint32_t packet_len, string interface)
{
    assert(packet);
    assert(0 < interface.length());

    //printf("*** -> Received packet of length %d \n", packet_len);
    //cout << "I am router " << m_id << endl;
    
    unsigned int minlength = sizeof(EthernetHeader);
    if(packet_len < minlength) 
    {
        fprintf(stderr, "Insufficient Length\n");
        return;
    }

    EthernetHeader* ethernet_ptr = (EthernetHeader*)packet;
    uint16_t ethtype =  ethernet_ptr->ether_type;

    if(ethtype == ethertype_ip) 
    {
    	//printf("Got the IP Packet \n");
        minlength += sizeof(IpHeader);
        if(packet_len < minlength)
        {
            fprintf(stderr, "Insufficient IP Header Length \n");
            return;
        }

        IpHeader* ip_ptr = (IpHeader*)(packet + sizeof(EthernetHeader));

        uint16_t temp_sum = ip_ptr->ip_sum; // Check sum check
        ip_ptr->ip_sum = 0;

        if(cksum(ip_ptr, sizeof(struct IpHeader)) != temp_sum)
        {
            fprintf(stderr, "IP Checksum error!\n");
            return;
        }
        ip_ptr->ip_sum = temp_sum;

        if(ip_ptr->ip_v != 4)
        {
            fprintf(stderr, "IP Version Error!\n");
            return;
        }

        if(ip_ptr->ip_hl != 4)
        {
            fprintf(stderr, "ip_hl Error!\n");
            return;
        }

        if(ip_ptr->ip_len != (packet_len - sizeof(EthernetHeader)))
        {
            fprintf(stderr, "ip_len Error!\n");
            return;
        }

        if(isThisIpForMe(packet) == false) // IP Forwarding
        {
            if(1 < ip_ptr->ip_ttl)
            {
                //print_hdr_eth(packet);
                //print_hdr_ip(packet + sizeof(sr_ethernet_hdr_t));
                RoutingTable* addr_box = longestPrefixMatch(ip_ptr->ip_dst);
                if(addr_box == NULL) // LPM miss
                {
                	//fprintf(stderr, "addr_box is NULL! \n");	
                    IcmpT11Header* new_icmp_hdr = createIcmpHeader(3, 0, ip_ptr);
                    uint8_t* new_buf = createIcmpResponse(packet, new_icmp_hdr, interface);
                    addPacketToOutputQueue(new_buf, sizeof(EthernetHeader)+sizeof(IpHeader)+sizeof(IcmpT11Header), interface);
                }
                else //LPM exists
                {
                	//printf("LPM Hit! \n");
                    ArpCacheEntry* arp_cache_entry = arpCacheLookUp(addr_box->next_hop);

                    if(arp_cache_entry != NULL) // cache hit
                    {
                        Interface* if_elem = getInterface(addr_box->interface_name);
                        assert(if_elem);
                        memcpy(ethernet_ptr->ether_shost, if_elem->mac_addr, ETHER_ADDR_LEN);
                        memcpy(ethernet_ptr->ether_dhost, arp_cache_entry->mac, ETHER_ADDR_LEN);

                        --(ip_ptr->ip_ttl);

                        ip_ptr->ip_sum = 0;
                        ip_ptr->ip_sum = cksum(ip_ptr, sizeof(IpHeader));

                        addPacketToOutputQueue(packet, packet_len, if_elem->name);
                    }
                    else // cache miss
                    {
                    	//printf("Cache Miss \n");
                        arpCacheQueueReq(addr_box->next_hop, packet, packet_len, addr_box->interface_name);
                    }
                }
            }
            else // TTL expired
            {
                //printf("TTL Expired \n");
                //print_hdr_eth(packet);
                //print_hdr_ip(packet + sizeof(sr_ethernet_hdr_t));
                /*
                IcmpT11Header* new_icmp_hdr = createIcmpHeader(11, 0, ip_ptr);
                uint8_t* new_buf = createIcmpResponse(packet, new_icmp_hdr, interface);
                addPacketToOutputQueue(new_buf, sizeof(EthernetHeader)+sizeof(IpHeader)+sizeof(IcmpT11Header), interface);
                */
                return;
            }
        }
        else // IP destined for me
        {
            //printf("IP destined for me \n");
            if(ip_ptr->ip_p != ip_protocol_icmp)
            {
                IcmpT11Header* new_icmp_hdr = createIcmpHeader(3, 3, ip_ptr);
                uint8_t* new_buf = createIcmpResponse(packet, new_icmp_hdr, interface);
                addPacketToOutputQueue(new_buf, sizeof(EthernetHeader)+sizeof(IpHeader)+sizeof(IcmpT11Header), interface);
            }
            else
            {
                IcmpT11Header* icmp_hdr_rcvd = (IcmpT11Header*)(packet + sizeof(EthernetHeader) + sizeof(IpHeader));
                if(icmp_hdr_rcvd->icmp_type == 8)
                {
                    uint8_t* new_buf = echoIcmp(packet);
                    addPacketToOutputQueue(new_buf, packet_len, interface);
                }
            }

        }
    }
    else if(ethtype == ethertype_arp) // ARP Request or Reply
    {
        //printf("ARP Packet Arrived \n");
        //print_hdr_eth(packet);
        //print_hdr_arp(packet + sizeof(sr_ethernet_hdr_t));
        minlength += sizeof(ArpHeader);

        if(packet_len < minlength)
        {
            fprintf(stderr, "Insufficient Length ETHERNET + ARP\n");
            return;
        }

        ArpHeader* arp_hdr = (ArpHeader*)(packet + sizeof(EthernetHeader));
      
        bool arp_for_me = isThisArpForMe(arp_hdr->ar_tip);

        if(arp_for_me == false)
        {
            fprintf(stderr, "ARP packet not destined for me\n");
            return;   
        }

        if(arp_hdr->ar_op == arp_op_request) // Arp Request To Me
        {
            //printf("ARP Request To Me \n");
            Interface* if_ptr = getInterface(interface);
            assert(if_ptr);
            handleArpRequestToMe(packet, if_ptr);
            addPacketToOutputQueue(packet, packet_len, interface);

        }
        else if(arp_hdr->ar_op == arp_op_reply)
        {
        	//printf("Arp Reply To Me \n");
			handleArpReplyToMe(packet);
        }
        else
        {
            fprintf(stderr, "ARP Type Specifying Error\n");
            return;  
        }
    }
    else
    {
        fprintf(stderr, "Type Specifying Error\n");
        return;   
    }
}