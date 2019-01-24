#include "host.h"
#include "router.h"
void* runAttackerRouter(void* input_router)
{
	struct timespec time_spec;
    struct timeval curr_timeval;

    const int WAIT_SEC_TIME = 0;
    const long WAIT_USEC_TIME = 1000000;

    AttackerRouter* router = (AttackerRouter*)input_router;
    pthread_cond_init(&router->m_input_queue_cv, NULL);
    pthread_mutex_init(&router->m_input_queue_mutex, NULL);

    while(true)
    {
    	//printf("Run Router Initiated \n");
    	gettimeofday(&curr_timeval, NULL);
    	
    	time_spec.tv_sec  = curr_timeval.tv_sec;
        time_spec.tv_nsec = curr_timeval.tv_usec * 1000;
        
        time_spec.tv_sec += WAIT_SEC_TIME;
        time_spec.tv_nsec += WAIT_USEC_TIME * 1000;

        if(1000000000 <= time_spec.tv_nsec)
        {
            time_spec.tv_sec++;
            time_spec.tv_nsec -= 1000000000;
        }

        router->arpCacheCleaning();
        
        pthread_mutex_lock(&router->m_input_queue_mutex);

        bool sth_to_do = false;

        for(auto it = router->m_input_queue.begin(); it != router->m_input_queue.end(); ++it)
        {
        	if(!it->second.empty())
        	{
        		sth_to_do = true; //It means at least one of the output queues is not empty so there is job to do
        		break;
        	}
        }
 
 		if(sth_to_do == false) // Nothing to do so makes this thread just sleep for a while
 		{
 			pthread_cond_timedwait(&router->m_input_queue_cv, &router->m_input_queue_mutex, &time_spec); 
 		}

 		for(auto it = router->m_input_queue.begin(); it != router->m_input_queue.end(); ++it)
        {
        	if(!it->second.empty()) //it->second is the input queue. 
        	{
        		while(!it->second.empty())
        		{
        			pair<uint32_t, uint8_t*> popped_pair = it->second.front();
        			it->second.pop();
        			SwitchPacket* switch_packet_ptr = new SwitchPacket;
        			switch_packet_ptr->buf = popped_pair.second;
        			switch_packet_ptr->len = popped_pair.first;
        			(switch_packet_ptr->iface).assign(it->first);
        			router->m_switch_queue.push(switch_packet_ptr);
        			/*
        			if(router->randomEarlyDetection() == false)
        			{
        				router->m_switch_queue.push(switch_packet_ptr);
        			}
        			else
        			{
        				printf("I will drop \n");
        			}
        			*/
        		}
        	}
        }

        pthread_mutex_unlock(&router->m_input_queue_mutex);

        while(!router->m_switch_queue.empty()) //This should be a member function of Router so the congestion control can be implemented. 
        {
        	SwitchPacket* switch_packet_ptr = router->m_switch_queue.front();
        	router->m_switch_queue.pop();
        	router->attackerHandlePacket(switch_packet_ptr->buf, switch_packet_ptr->len, switch_packet_ptr->iface);
        }

        router->sweepArpReqs();

        router->sendToTheNextHop();
    }
    return NULL;
}