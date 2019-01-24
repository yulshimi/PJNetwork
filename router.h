#ifndef __ROUTER_H__
#define __ROUTER_H__

#define ETHER_ADDR_LEN 6
#define MAX_QUEUE_LEN 255
#include <stdio.h>
#include <time.h>
#include <stdint.h>
#include <pthread.h>
#include <sys/time.h>
#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <queue>
#include <cstring>
#include <string.h>
#include <thread>
#include <fstream>
#include <sstream>
#include "utility.h"
#define SPLICE_SIZE 7
#define MAC_SPLICE_SIZE 6
#define MAX_TH 8
#define MIN_TH 2
#define AVG_Q_LEN 3
#define ARP_CACHE_EXPIRED 20
using namespace std;
struct RoutingTable // Array or Vector?
{
    uint32_t dest_addr;
    uint32_t next_hop;
    uint32_t mask;
    bool in_use; //This path is in use
    string   interface_name;
};

struct Interface
{
    string name; //Key
    uint8_t mac_addr[ETHER_ADDR_LEN];
    uint32_t ip;
};

struct ArpCacheEntry 
{
    unsigned char mac[ETHER_ADDR_LEN]; 
    uint32_t ip; //Key                
    time_t added;         
};

struct RouterTrie
{
    struct RouterTrie* child_trie[2];
    RoutingTable* dest_addr; 
};
typedef struct RouterTrie RouterTrie;


struct ArpReq
{
    uint32_t ip; //The IP address that an ARP request has been sent                               
    uint32_t times_sent; //How many times the ARP request has been sent so far
    time_t sent; //When the ARP request was sent   
    string iface; //The name of the interface that the ARP request has been sent(should be the same as "iface" of ReqPacket)  
};
typedef struct ArpReq ArpReq; 


struct ReqPacket 
{
    uint8_t* buf; //A packet waiting to be sent             
    uint32_t len; //A length of a packet(buf)          
    string iface; //The name of the interface that a packet(buf) is going to be sent(for ReqPacket)
    			  //The name of the interface that a packet(buf) came in(for SwitchPacket)               
};
typedef struct ReqPacket ReqPacket;
typedef struct ReqPacket SwitchPacket;

//typedef uint32_t(*acceptPacketsFunctionPtr)(pair<uint32_t, uint8_t*>, queue<pair<uint32_t, uint8_t*> >*);

struct SendThreadArg
{
	//acceptPacketsFunctionPtr accept_to_queue_function;
	queue<pair<uint32_t, uint8_t*> >* src_queue_ptr; //The source queue
	queue<pair<uint32_t, uint8_t*> >* dst_queue_ptr; //The destination queue
	pthread_mutex_t* dst_queue_mutex;
	pthread_cond_t* dst_queue_cv;
};

struct Adjacent
{
	void* object_ptr; //a pointer to a router or a host
	bool router_or_host; //true if it is a router
};

class Router
{
public:
	Router(){}
	Router(ifstream& in_file, uint32_t r_id);
	pthread_mutex_t m_input_queue_mutex;
	pthread_cond_t m_input_queue_cv;

	queue<SwitchPacket*> m_switch_queue;
	vector<RoutingTable*> m_routing_table;
	unordered_map<string, queue<pair<uint32_t, uint8_t*> > > m_input_queue;
	unordered_map<string, queue<pair<uint32_t, uint8_t*> > > m_output_queue;

	unordered_map<uint32_t, ArpCacheEntry*> m_arp_cache;

	unordered_map<string, SendThreadArg*> m_send_thread_arg_list; 

	unordered_map<string, Interface*> m_interface_list;

	unordered_map<uint32_t, queue<ReqPacket* > > m_arpreq_queue;
	unordered_map<uint32_t, ArpReq*> m_arpreq_times_list;

public:
	void handlePacket(uint8_t* packet, uint32_t packet_len, string interface); //It takes care of inserting a packet into the output queue
	bool isThisIpForMe(uint8_t* packet);
	void addPacketToOutputQueue(uint8_t* buf, uint32_t len, string interface);
	void arpCacheQueueReq(uint32_t req_ip, uint8_t* packet, uint32_t packet_len, string interface);
	ArpCacheEntry* arpCacheLookUp(uint32_t next_hop_ip);
	Interface* getInterface(string interface);
	bool isThisArpForMe(uint32_t target_ip);
	void handleArpRequestToMe(uint8_t* buf, Interface* ifPtr);
	void handleArpReplyToMe(uint8_t* packet);
	void sweepArpReqs();
	uint8_t* echoIcmp(uint8_t* buf);
	IcmpT11Header* createIcmpHeader(uint8_t type, uint8_t code, IpHeader* old_ip_hdr);
	uint8_t* createArpRequestHeader(Interface* in_if, uint32_t target_ip);
	ArpHeader* createArpHeader(Interface* in_if, uint32_t target_ip);
	string requestInterfaceName(uint8_t* in_mac_addr);
	uint32_t acceptPacketsToQueue(pair<uint32_t, uint8_t*> new_packet);
	bool sendToTheNextHop();
	bool randomEarlyDetection();
	void trieConstructor();
	RouterTrie* trieNodeConstructor();
	void trieInsert(RoutingTable* rtPtr); 
	RoutingTable* longestPrefixMatch(uint32_t dest_ip_addr);
	void inputToOutputQueue();
	uint32_t acceptPacketsToQueue(pair<uint32_t, uint8_t*> new_packet, queue<pair<uint32_t, uint8_t*> >* queue_ptr);
	uint8_t* createIcmpResponse(uint8_t* buf, IcmpT11Header* icmp_hdr, string iface);
	queue<pair<uint32_t, uint8_t*> >* getOutputQueue(string interface);
	void addThreadArg(string interface, SendThreadArg* send_arg);
	void flipRoutingTable(uint32_t dest_ip_addr);
	void addRoutingTable(RoutingTable* rt)
	{
		m_routing_table.push_back(rt);
	}
	Interface* getInterfaceByNextHop(uint32_t next_hop_ip);
	uint32_t getInterfaceIp(string interface);
	void arpCacheCleaning();
protected:
	RouterTrie* m_address_trie;
	uint32_t m_num_of_interfaces;
	uint32_t m_avg_queue_length;
	uint32_t m_min_threshold;
	uint32_t m_max_threshold;
	bool m_border;
	uint32_t m_id;
};

class AttackerRouter : public Router
{
public:
	AttackerRouter(){}
	AttackerRouter(ifstream& in_file, uint32_t r_id);
	void attackerHandlePacket(uint8_t* packet, uint32_t packet_len, string interface);
	bool attackObject(uint8_t* buf);
	void createAttackMessage(uint8_t* buf_1, uint8_t* buf_2);
	bool alterSeqAckNum(uint8_t* buf);
	bool dropAck(uint8_t* buf);
private:
	unordered_map<uint32_t, uint8_t> m_hack_ack_data;
	unordered_map<uint32_t, bool> m_seq_ack_control_flag;
};

void* sendThread(void* thread_arg);
#endif