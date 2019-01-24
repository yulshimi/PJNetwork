#include "host.h"
#include "router.h"
void* runReceiver(void* input_receiver)
{
	struct timespec   time_spec;
    struct timeval    curr_timeval;

    const int WAIT_SEC_TIME = 0;
    const long WAIT_USEC_TIME = 1000000;

    ReceiverHost* receiver = (ReceiverHost*)input_receiver;
    pthread_cond_init(&receiver->m_input_queue_cv, NULL);
    pthread_mutex_init(&receiver->m_input_queue_mutex, NULL);

    while(true)
    {
        //printf("Run Receiver Initiated \n");
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

        pthread_mutex_lock(&receiver->m_input_queue_mutex);

        bool sth_to_do = false;

        if(!receiver->m_input_queue.empty())
        {
            sth_to_do = true;
        }
 
 		if(sth_to_do == false) // Nothing to do so makes this thread just sleep for a while
 		{
 			pthread_cond_timedwait(&receiver->m_input_queue_cv, &receiver->m_input_queue_mutex, &time_spec); 
 		}

        if(!receiver->m_input_queue.empty())
        {
            receiver->handlePacket();
        }
        
        pthread_mutex_unlock(&receiver->m_input_queue_mutex);

        if(!receiver->isOutputQueueEmpty())
        {
            receiver->sendMessageToRouter();
        }
    }
    return NULL;
}











