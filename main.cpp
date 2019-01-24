//#include "router.h"
//#include "host.h"
#include "AS.h"
#include <pthread.h>
#include <string>
#include <stdio.h>
#include <fstream>
#include <ios>   
#include <limits>     
#define NUM_OF_ROUTERS 7
#define NUM_OF_RECEIVERS 2
#define NUM_OF_AS 5

void* runRouter(void* input_router);
void* runAttackerRouter(void* input_router);
void* runSender(void* input_sender);
void* runReceiver(void* input_receiver);
int main()
{
	ifstream router_file[NUM_OF_ROUTERS];
	for(int i=0; i < NUM_OF_ROUTERS; ++i)
	{
		string file_name = "router_" + to_string(i+1) + ".txt";
		router_file[i].open(file_name);
	}

	ifstream sender_file;
	sender_file.open("sender.txt");

	ifstream receiver_file[NUM_OF_RECEIVERS];
	for(int i=0; i < NUM_OF_RECEIVERS; ++i)
	{
		string file_name = "receiver_" + to_string(i+1) + ".txt";
		receiver_file[i].open(file_name);
	}

	Router* router[NUM_OF_ROUTERS];
	for(int i=0; i < NUM_OF_ROUTERS; ++i)
	{
		if(i == 3)
		{
			router[i] = new AttackerRouter(router_file[i], i+1);
		}
		else
		{
			router[i] = new Router(router_file[i], i+1);
		}
	}
	
	SenderHost* sender = new SenderHost(sender_file);

	ReceiverHost* receiver[NUM_OF_RECEIVERS];
	for(int i=0; i < NUM_OF_RECEIVERS; ++i)
	{
		receiver[i] = new ReceiverHost(receiver_file[i]);
	}

	connectHostToRouter(sender, router[0], "eth1");
	connectHostToRouter(receiver[0], router[3], "eth3");
	connectHostToRouter(receiver[1], router[6], "eth3");
	//connectRouterToRouter(router[0], router[3], "eth3", "eth1");
	connectRouterToRouter(router[0], router[1], "eth3", "eth1");
	connectRouterToRouter(router[1], router[2], "eth3", "eth1");
	connectRouterToRouter(router[2], router[3], "eth3", "eth1");
	connectRouterToRouter(router[0], router[4], "eth2", "eth1");
	connectRouterToRouter(router[4], router[5], "eth3", "eth1");
	connectRouterToRouter(router[5], router[6], "eth2", "eth1");
	connectRouterToRouter(router[5], router[3], "eth3", "eth2");
	
	AS* as[NUM_OF_AS];

	vector<Router*> router_vector;

	router_vector.push_back(router[0]);
	as[0] = new AS("AS1", false, true, router_vector);
	router_vector.clear();

	router_vector.push_back(router[1]);
	router_vector.push_back(router[2]);
	as[1] = new AS("AS2", false, true, router_vector);
	router_vector.clear();
	
	router_vector.push_back(router[3]);
	as[2] = new AS("AS3", true, true, router_vector);
	router_vector.clear();

	router_vector.push_back(router[4]);
	router_vector.push_back(router[5]);
	as[3] = new AS("AS4", false, true, router_vector);
	router_vector.clear();

	router_vector.push_back(router[6]);
	as[4] = new AS("AS5", true, false, router_vector);
	router_vector.clear();

	//m_path_list_update
	vector<string> path_vector;
	path_vector.push_back("AS2");
	path_vector.push_back("AS3");
	as[0]->addPathList(2889876234, path_vector, true);
	path_vector.clear();

	path_vector.push_back("AS4");
	path_vector.push_back("AS3");
	as[0]->addPathList(2889876234, path_vector, false);
	path_vector.clear();

	path_vector.push_back("AS3");
	as[1]->addPathList(2889876234, path_vector, true);
	path_vector.clear();

	path_vector.push_back("AS3");
	as[3]->addPathList(2889876234, path_vector, true);
	path_vector.clear();

	//m_adjacent_as_list update
	as[0]->addAdjacentAS("AS2", as[1]);
	as[0]->addAdjacentAS("AS4", as[3]);

	as[1]->addAdjacentAS("AS1", as[0]);
	as[1]->addAdjacentAS("AS3", as[2]);

	as[2]->addAdjacentAS("AS2", as[1]);
	as[2]->addAdjacentAS("AS4", as[3]);

	as[3]->addAdjacentAS("AS1", as[0]);
	as[3]->addAdjacentAS("AS3", as[2]);
	as[3]->addAdjacentAS("AS5", as[4]);

	as[4]->addAdjacentAS("AS4", as[3]);

	//m_reachable_dest_ip
	as[0]->addReachableDestination(2889876234);
	as[1]->addReachableDestination(2889876234);
	as[2]->addReachableDestination(2889876234);
	as[3]->addReachableDestination(2889876234);
	as[4]->addReachableDestination(2889876244);

	//m_border_router_map update
	connectAStoAS(as[0], as[1], "eth3", "eth1", router[0], router[1]);

	connectAStoAS(as[0], as[3], "eth2", "eth1", router[0], router[4]);

	connectAStoAS(as[1], as[2], "eth3", "eth1", router[2], router[3]);

	connectAStoAS(as[3], as[2], "eth3", "eth2", router[5], router[3]);

	connectAStoAS(as[3], as[4], "eth2", "eth1", router[5], router[6]);

	vector<AS*> as_vec;
	for(int i=0; i < NUM_OF_AS; ++i)
	{
		as_vec.push_back(as[i]);
	}
	initializeGlobalList(as_vec);

	pthread_t router_thread[NUM_OF_ROUTERS];
	pthread_t sender_thread;
	pthread_t receiver_thread[NUM_OF_RECEIVERS];

	for(int i=0; i < NUM_OF_ROUTERS; ++i)
	{
		if(i == 3)
		{
			pthread_create(&router_thread[i], NULL, runAttackerRouter, (void*)router[i]);	
		}
		else
		{
			pthread_create(&router_thread[i], NULL, runRouter, (void*)router[i]);	
		}	
	}

	pthread_create(&sender_thread, NULL, runSender, (void*)sender);

	for(int i=0; i < NUM_OF_RECEIVERS; ++i)
	{
		pthread_create(&receiver_thread[i], NULL, runReceiver, (void*)receiver[i]);
	}

	while(true)
	{
		string user_input;
		int user_int_input;
		char trash_char;
		printf("Select a destination: \n");
		printf("1. Receiver_1: 172.64.3.10 \n");
		printf("2. Receiver_2: 172.64.3.20 \n");
		printf("Or to add or withdraw the Autonomous System 5: \n");
		printf("3. Add the Autonomous System 5 \n");
		printf("4. Withdraw the Autonomous System 5 \n");

		cin >> user_int_input;
		cin.ignore(numeric_limits<streamsize>::max(),'\n');
		if(user_int_input == 1 || user_int_input == 2)
		{
			printf("Enter a message: \n");
			getline(cin, user_input);
			pthread_mutex_lock(&sender->m_user_queue_mutex);
			if(user_int_input == 1)
			{
				sender->m_user_input_queue.push(make_pair(2889876234, user_input)); //IP address should be added
			}
			else
			{
				sender->m_user_input_queue.push(make_pair(2889876244, user_input));	
			}
			pthread_mutex_unlock(&sender->m_user_queue_mutex);
			pthread_cond_signal(&sender->m_user_queue_cv);
		}
		else if(user_int_input == 3 || user_int_input == 4)
		{
			if(user_int_input == 3)
			{
				as[3]->hookUpRequest(as[4]);
			}
			else
			{
				as[4]->withdrawl();
			}
		}
		else{}
	}

	return 0;
}