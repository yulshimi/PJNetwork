#ifndef __HOST_H__
#define __HOST_H__

#include <stdio.h>
#include <time.h>
#include <stdint.h>
#include <pthread.h>
#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <queue>
#include "router.h"
//#include "AS.h"
using namespace std;
class Host
{
public:
	Host(){}
	Host(ifstream& in_file, bool sender);
	//virtual void sendMessage(string in_msg){}
	void handleArpRequestToMe(uint8_t* buf);
	//void sendMessageToRouter();
	bool isOutputQueueEmpty()
	{
		return m_output_queue.empty();
	}
	void setRouterInterface(string interface)
	{
		m_router_interface.assign(interface);
	}
	void setRouterMacAddr(uint8_t* mac)
	{
		memcpy(m_router_mac_addr, mac, ETHER_ADDR_LEN);
	}
	void setRouter(Router* router)
	{
		m_router = router;
	}
	bool isItDestinedForMe(uint8_t* buf);
public:
	queue<pair<uint32_t, uint8_t*> > m_input_queue;
	pthread_mutex_t m_input_queue_mutex;
	pthread_cond_t m_input_queue_cv;
	bool m_sender;
protected:
	uint32_t m_ip_addr;
	uint8_t m_mac_addr[ETHER_ADDR_LEN]; //The mac address of the host
	uint8_t m_router_mac_addr[ETHER_ADDR_LEN]; //The mac address of the connected router
	string m_router_interface; //The name of the interface of the router that a packet is going to be sent or come in
	queue<pair<uint32_t, uint8_t*> > m_output_queue; //It needs to be replaced by an array to implement SWP
	Router* m_router; //A router directly connected to a host
};

#define MAX_QUEUE_SIZE 256
struct SenderBlock // for each receiver
{
    uint8_t SWS;
    uint8_t LAR;
    uint8_t LFS;
    uint8_t SEQ;
    queue<pair<uint32_t, uint8_t*> > pending_frame_list; //first = length, second = buf
    FrameTime sender_queue[MAX_QUEUE_SIZE];
};
typedef struct SenderBlock SenderBlock;

class SenderHost: public Host
{
public:
	SenderHost(){}
	SenderHost(ifstream& in_file);
	void sendMessageToRouter();
	//virtual void sendMessage(string in_msg);
	//void addMessageToQueue(uint8_t* in_msg, uint32_t len, uint32_t dest_ip_addr);
	void handleUserInput(uint32_t dest_ip, string message);
	void addMessageToPendingList(Frame* frame);
	void senderFrameConstructor(Frame* input_frame, uint8_t marker, uint32_t src_ip, uint32_t dest_ip);
	void handlePendingFrames();
	void handleTimeoutFrames();
	void handleTimeoutHelper(SenderBlock* curr_block, uint8_t index);
	void handleIncomingRouterMessage();
	void handleIcmpMessage(uint8_t* buf);
public:
	pthread_mutex_t m_user_queue_mutex;
	pthread_cond_t m_user_queue_cv;
	queue<pair<uint32_t, string> > m_user_input_queue; //Pair.first = the destination IP address, Pair.second = the message
private:
	unordered_map<uint32_t, SenderBlock*> m_sender_block_list; //key: the ip address of each receiver
};

struct RecvInfo
{
	bool ack_marked;
	pair<uint32_t, uint8_t*> recv_elem;
};

struct ReceiverBlock
{
    uint8_t RWS;
    uint8_t NFE;
    RecvInfo receiver_queue[MAX_QUEUE_SIZE];    
};
typedef struct ReceiverBlock ReceiverBlock;

class ReceiverHost: public Host
{
public:
	ReceiverHost(){}
	ReceiverHost(ifstream& in_file);
	void sendMessageToRouter();
	//virtual void sendMessage(string in_msg);
	void handlePacket();
	void printOutMessage();
	uint8_t* ackFrameConstructor(uint32_t len, uint8_t* buf);
	void printOutMessage(ReceiverBlock* curr_block_ptr, uint8_t index);
private:
	unordered_map<uint32_t, ReceiverBlock*> m_receiver_block_list;
};

void connectRouterToRouter(Router* router_1, Router* router_2, string if_name_1, string if_name_2);
void connectHostToRouter(Host* host, Router* router, string interface);
//void connectSenderToRouter(SenderHost* sender, Router* router, string interface);
//void connectReceiverToRouter(ReceiverHost* receiver, Router* router, string interface);
#endif