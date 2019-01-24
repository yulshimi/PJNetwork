#include "host.h"
#include "router.h"
void* runSender(void* input_sender)
{
	struct timespec time_spec;
    struct timeval curr_timeval;

    const int WAIT_SEC_TIME = 0;
    const long WAIT_USEC_TIME = 1000000;

    SenderHost* sender = (SenderHost*)input_sender;
    pthread_cond_init(&sender->m_user_queue_cv, NULL);
    pthread_mutex_init(&sender->m_user_queue_mutex, NULL);

    while(true)
    {
        sender->handleIncomingRouterMessage();

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

        pthread_mutex_lock(&sender->m_user_queue_mutex);

        bool sth_to_do = false;

        if(!sender->m_user_input_queue.empty())
        {
            sth_to_do = true;
        }
 
 		if(sth_to_do == false) // Nothing to do so makes this thread just sleep for a while
 		{
 			pthread_cond_timedwait(&sender->m_user_queue_cv, &sender->m_user_queue_mutex, &time_spec); 
 		}

        while(!sender->m_user_input_queue.empty())
        {
            pair<uint32_t, string> user_input_str = sender->m_user_input_queue.front();
            sender->m_user_input_queue.pop();
            sender->handleUserInput(user_input_str.first, user_input_str.second);
        }

        pthread_mutex_unlock(&sender->m_user_queue_mutex);
      
        sender->handlePendingFrames();

        //sender->handleTimeoutFrames();

        sender->sendMessageToRouter();
    }
    return NULL;
}


















