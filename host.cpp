#include "host.h"
#include <stack>
#define RWS_SIZE 8
#define SWS_SIZE 8

void connectRouterToRouter(Router* router_1, Router* router_2, string if_name_1, string if_name_2)
{
	SendThreadArg* send_thread_arg_1 = new SendThreadArg;
	SendThreadArg* send_thread_arg_2 = new SendThreadArg;

	send_thread_arg_1->src_queue_ptr = &(router_1->m_output_queue[if_name_1]);
	send_thread_arg_1->dst_queue_ptr = &(router_2->m_input_queue[if_name_2]);
	send_thread_arg_1->dst_queue_mutex = &(router_2->m_input_queue_mutex);
	send_thread_arg_1->dst_queue_cv = &(router_2->m_input_queue_cv);

	send_thread_arg_2->src_queue_ptr = &(router_2->m_output_queue[if_name_2]);
	send_thread_arg_2->dst_queue_ptr = &(router_1->m_input_queue[if_name_1]);
	send_thread_arg_2->dst_queue_mutex = &(router_1->m_input_queue_mutex);
	send_thread_arg_2->dst_queue_cv = &(router_1->m_input_queue_cv);

	router_1->m_send_thread_arg_list.insert(make_pair(if_name_1, send_thread_arg_1));
	router_2->m_send_thread_arg_list.insert(make_pair(if_name_2, send_thread_arg_2));
}

void connectHostToRouter(Host* host, Router* router, string interface)
{
	host->setRouter(router);
    host->setRouterInterface(interface);
    Interface* if_ptr = router->getInterface(interface);
    host->setRouterMacAddr(if_ptr->mac_addr);

    SendThreadArg* thread_arg = new SendThreadArg;
    thread_arg->src_queue_ptr = router->getOutputQueue(interface);
    thread_arg->dst_queue_ptr = &(host->m_input_queue);
    thread_arg->dst_queue_mutex = &(host->m_input_queue_mutex);
    if(!host->m_sender)
    {
    	thread_arg->dst_queue_cv = &(host->m_input_queue_cv);
    }
    else
    {
    	thread_arg->dst_queue_cv = NULL;
    }

    router->addThreadArg(interface, thread_arg);
}

Host::Host(ifstream& in_file, bool sender)
{
	string line;
	m_sender = sender;
	while(getline(in_file, line))
	{
		vector<string> spliced = splice(line, ' ');

		vector<string> ip_spliced_str = splice(spliced[0], '.');
		vector<uint32_t> ip_spliced_uint = fromStrToUintVector(ip_spliced_str);
		uint32_t sender_ip = convertTo32Bits(ip_spliced_uint);
		m_ip_addr = sender_ip;

		vector<string> mac_spliced_str = splice(spliced[1], ':');
		assert(mac_spliced_str.size() == MAC_SPLICE_SIZE);

		for(int i=0; i < mac_spliced_str.size(); ++i)
		{
			int mac_elem = stoi(mac_spliced_str[i], 0, 16);
			uint8_t char_elem = (uint8_t)mac_elem;
			m_mac_addr[i] = char_elem;
		}
	}
}

bool Host::isItDestinedForMe(uint8_t* buf)
{
	IpHeader* ip_hdr = (IpHeader*)(buf + sizeof(EthernetHeader));
	return m_ip_addr == ip_hdr->ip_dst;
}

//Modify it later
SenderHost::SenderHost(ifstream& in_file):Host(in_file, true)
{
	/*
	SenderBlock* new_sender_block = new SenderBlock;
	new_sender_block->SWS = SWS_SIZE;
    new_sender_block->LAR = -1;
    new_sender_block->LFS = -1;
    new_sender_block->SEQ = 0;
    for(int i=0; i < MAX_QUEUE_SIZE; ++i)
    {
    	new_sender_block->sender_queue[i].ack_marked = false;
    }
    m_sender_block_list.insert(make_pair(2889876234, new_sender_block));
    m_sender_block_list.insert(make_pair(2889876244, new_sender_block));
    */
}

ReceiverHost::ReceiverHost(ifstream& in_file):Host(in_file, false)
{
	ReceiverBlock* new_receiver_block = new ReceiverBlock;
	new_receiver_block->RWS = RWS_SIZE;
    new_receiver_block->NFE = 0;
    for(int i=0; i < MAX_QUEUE_SIZE; ++i)
    {
    	new_receiver_block->receiver_queue[i].ack_marked = false;
    }
    m_receiver_block_list.insert(make_pair(3232236034, new_receiver_block));
}

void Host::handleArpRequestToMe(uint8_t* buf)
{
	EthernetHeader* ethernet_hdr = (EthernetHeader*)buf;
    ArpHeader* arp_hdr = (ArpHeader*)(buf + sizeof(EthernetHeader));

    memcpy(ethernet_hdr->ether_dhost, ethernet_hdr->ether_shost, ETHER_ADDR_LEN);
    memcpy(ethernet_hdr->ether_shost, m_mac_addr, ETHER_ADDR_LEN);

    memcpy(arp_hdr->ar_tha, arp_hdr->ar_sha, ETHER_ADDR_LEN);
    memcpy(arp_hdr->ar_sha, m_mac_addr, ETHER_ADDR_LEN);

    arp_hdr->ar_tip = arp_hdr->ar_sip;
    arp_hdr->ar_sip = m_ip_addr;

    arp_hdr->ar_op = arp_op_reply;
}

void ReceiverHost::printOutMessage(ReceiverBlock* curr_block, uint8_t index)
{
	printf("The message from the sender \n");
	int max_loop_count = 256;
	stack<Frame*> frame_stack;
	while(0 < max_loop_count)
	{
		pair<uint32_t, uint8_t*> search_elem = curr_block->receiver_queue[index--].recv_elem;
        Frame* search_frame = (Frame*)(search_elem.second + sizeof(EthernetHeader) + sizeof(IpHeader));
        //printf("Push to the stack: %s \n", search_frame->data);
        frame_stack.push(search_frame);
        if(search_frame->start_of_message)
        {
        	break;
        }
        --max_loop_count;
	}
	while(!frame_stack.empty())
	{
		Frame* stack_elem = frame_stack.top();
		frame_stack.pop();
		printf("%s", stack_elem->data);
	}
	printf("\n");
}

void ReceiverHost::handlePacket()
{
	while(!m_input_queue.empty())
	{
		pair<uint32_t, uint8_t*> elem = m_input_queue.front();
		m_input_queue.pop();
		EthernetHeader* ethernet_hdr = (EthernetHeader*)elem.second;
		if(ethernet_hdr->ether_type == ethertype_arp)
		{
			handleArpRequestToMe(elem.second);
			m_output_queue.push(elem);
		}
		else
		{
			IpHeader* ip_hdr = (IpHeader*)(elem.second + sizeof(EthernetHeader));
			Frame* frame = (Frame*)(elem.second + sizeof(EthernetHeader) + sizeof(IpHeader));
            //printf("Received: %s \n", frame->data);
            if(ip_hdr->ip_dst == m_ip_addr)
            {
                ReceiverBlock* curr_block = m_receiver_block_list[ip_hdr->ip_src];
                uint8_t seq_num = frame->seq_ack_num;
                //printf("Received seq number: %d \n", seq_num);
                uint8_t curr_NFE = curr_block->NFE;
                uint8_t curr_LAF = curr_block->NFE + curr_block->RWS - 1;
                if((curr_NFE <= seq_num && seq_num <= curr_LAF) || ((curr_LAF < curr_NFE) && (curr_NFE <= seq_num || seq_num <= curr_LAF)))
                {
                	RecvInfo new_info;
                	new_info.ack_marked = true;
                	new_info.recv_elem = elem;

                    curr_block->receiver_queue[seq_num] = new_info;

                    if(seq_num == curr_NFE)
                    {
                        uint8_t index = seq_num;
                        while(curr_block->receiver_queue[index].ack_marked)
                        {
                        	pair<uint32_t, uint8_t*> search_elem = curr_block->receiver_queue[index].recv_elem;
                        	Frame* search_frame = (Frame*)(search_elem.second + sizeof(EthernetHeader) + sizeof(IpHeader));
                            if(search_frame->end_of_message)
                            {
                                printOutMessage(curr_block, index);
                            }
                            curr_block->receiver_queue[index++].ack_marked = false;
                            ++curr_block->NFE;
                        }
                    }
                }
                uint8_t* ack_frame = ackFrameConstructor(elem.first, elem.second);
             
                m_output_queue.push(make_pair(elem.first, ack_frame));
            }
        }
	}
}

//create an ACK based on the input buf
uint8_t* ReceiverHost::ackFrameConstructor(uint32_t len, uint8_t* buf)
{
	uint8_t* new_ack = (uint8_t*)malloc(len);
	EthernetHeader* new_ethernet_hdr = (EthernetHeader*)new_ack;
	IpHeader* new_ip_hdr = (IpHeader*)(new_ack + sizeof(EthernetHeader));
	Frame* new_frame = (Frame*)(new_ack + sizeof(EthernetHeader) + sizeof(IpHeader));

	memcpy(new_ethernet_hdr->ether_shost, m_mac_addr, ETHER_ADDR_LEN);
	memcpy(new_ethernet_hdr->ether_dhost, m_router_mac_addr, ETHER_ADDR_LEN);
	new_ethernet_hdr->ether_type = ethertype_ip;

	IpHeader* old_ip_hdr = (IpHeader*)(buf + sizeof(EthernetHeader));
	memcpy((uint8_t*)new_ip_hdr, (uint8_t*)old_ip_hdr, sizeof(IpHeader));
	new_ip_hdr->ip_ttl = 250;
	new_ip_hdr->ip_sum = 0;
	new_ip_hdr->ip_src = m_ip_addr;
	new_ip_hdr->ip_dst = old_ip_hdr->ip_src;
	new_ip_hdr->ip_sum = cksum(new_ip_hdr, sizeof(IpHeader));

	Frame* old_frame = (Frame*)(buf + sizeof(EthernetHeader) + sizeof(IpHeader));
	new_frame->src_ip = m_ip_addr;
	new_frame->dest_ip = old_ip_hdr->ip_src;
	new_frame->seq_ack_num = old_frame->seq_ack_num;
	new_frame->is_it_ack = true;
	new_frame->start_of_message = false;
	new_frame->end_of_message = false;
	new_frame->crc = 0;
	return new_ack;
}

void SenderHost::sendMessageToRouter()
{
	//printf("Sending To The Router...... \n");
	pthread_mutex_lock(&m_router->m_input_queue_mutex);

	unordered_map<string, queue<pair<uint32_t, uint8_t*> > >::iterator it = m_router->m_input_queue.find(m_router_interface);
	if(it == m_router->m_input_queue.end())
	{
		fprintf(stderr, "Cannot send a message to a connected router\n");
		pthread_mutex_unlock(&m_router->m_input_queue_mutex);
		return;
	}

	while(!m_output_queue.empty())
	{
		pair<uint32_t, uint8_t*> elem = m_output_queue.front();
		m_output_queue.pop();
		//m_router->acceptPacketsToQueue(elem, &(it->second));
		it->second.push(elem);
	}

	pthread_mutex_unlock(&m_router->m_input_queue_mutex);
	//printf("Send To The Router End \n");
}

void ReceiverHost::sendMessageToRouter()
{
	pthread_mutex_lock(&m_router->m_input_queue_mutex);

	unordered_map<string, queue<pair<uint32_t, uint8_t*> > >::iterator it = m_router->m_input_queue.find(m_router_interface);
	if(it == m_router->m_input_queue.end())
	{
		fprintf(stderr, "Cannot send a message to a connected router\n");
		pthread_mutex_unlock(&m_router->m_input_queue_mutex);
		return;
	}

	while(!m_output_queue.empty())
	{
		pair<uint32_t, uint8_t*> elem = m_output_queue.front();
		m_output_queue.pop();
		//m_router->acceptPacketsToQueue(elem, &(it->second));
		it->second.push(elem);
	}

	pthread_mutex_unlock(&m_router->m_input_queue_mutex);
}
//It assumes that in_msg is terminated with '\0'
//It adds a message to a connected router
//SWP is not yet implemented
//Its job is to construct the complete content and then push it into the appropriate pending frame list
void SenderHost::addMessageToPendingList(Frame* frame)
{
	uint32_t dest_ip_addr = frame->dest_ip;
	uint8_t* buf = (uint8_t*)malloc(sizeof(EthernetHeader) + sizeof(IpHeader) + sizeof(Frame));
	uint32_t buf_len = sizeof(EthernetHeader) + sizeof(IpHeader) + sizeof(Frame);

	EthernetHeader* ethernet_hdr = (EthernetHeader*)buf;
	IpHeader* ip_header = (IpHeader*)(buf + sizeof(EthernetHeader));
	uint8_t* tcp_header = (uint8_t*)(buf + sizeof(EthernetHeader) + sizeof(IpHeader));

	memcpy(ethernet_hdr->ether_dhost, m_router_mac_addr, ETHER_ADDR_LEN);
	memcpy(ethernet_hdr->ether_shost, m_mac_addr, ETHER_ADDR_LEN);
	ethernet_hdr->ether_type = ethertype_ip;

	ip_header->ip_v = 4;
	ip_header->ip_hl = 4;
	ip_header->ip_p = ip_protocol_tcp;
	ip_header->ip_src = m_ip_addr;
	ip_header->ip_dst = dest_ip_addr;
	ip_header->ip_ttl = 250;
	ip_header->ip_len = sizeof(IpHeader) + sizeof(Frame);
	ip_header->ip_sum = 0;
	ip_header->ip_sum = cksum(ip_header, sizeof(IpHeader));

	memcpy(tcp_header, (uint8_t*)frame, sizeof(Frame));

	delete frame;

	unordered_map<uint32_t, SenderBlock*>::iterator it = m_sender_block_list.find(dest_ip_addr);
	assert(it != m_sender_block_list.end());
	it->second->pending_frame_list.push(make_pair(buf_len, buf));
	//printf("addMessageToQueue Done\n");
}

void SenderHost::handleUserInput(uint32_t dest_ip, string message)
{
	unordered_map<uint32_t, SenderBlock*>::iterator it = m_sender_block_list.find(dest_ip);
	if(it == m_sender_block_list.end())//Handle invalid destination IP address
	{
		//fprintf(stderr, "Invalid Receiver ID in addMessageToPendingList function \n");
		SenderBlock* new_sender_block = new SenderBlock;
		new_sender_block->SWS = SWS_SIZE;
    	new_sender_block->LAR = -1;
    	new_sender_block->LFS = -1;
    	new_sender_block->SEQ = 0;
    	for(int i=0; i < MAX_QUEUE_SIZE; ++i)
    	{
    		new_sender_block->sender_queue[i].ack_marked = false;
    	}
		m_sender_block_list.insert(make_pair(dest_ip, new_sender_block));
	}
	uint32_t msg_length = message.length();
    if(FRAME_PAYLOAD_SIZE < msg_length)
    {
        uint32_t num_of_chunks = msg_length / (FRAME_PAYLOAD_SIZE - 1) + 1;
        uint32_t remainder = msg_length % (FRAME_PAYLOAD_SIZE - 1);
        for(uint i=0; i < num_of_chunks; ++i)
        {
            Frame* frame_ptr = new Frame;
            uint j = 0;
            uint8_t position_marker;
            if(i < num_of_chunks - 1)
            {
                position_marker = 2;
                for(j=0; j < FRAME_PAYLOAD_SIZE-1; ++j)
                {
                    frame_ptr->data[j] = message[j + i*(FRAME_PAYLOAD_SIZE-1)];
                }
                frame_ptr->data[FRAME_PAYLOAD_SIZE-1] = '\0';
                if(i == 0)
                {
                        position_marker = 1;
                }
            }
            else //The last chunk
            {
                position_marker = 3;
                if(remainder == 0)
                {
                    for(j=0; j < FRAME_PAYLOAD_SIZE-1; ++j)
                    {
                        frame_ptr->data[j] = message[j + i*(FRAME_PAYLOAD_SIZE-1)];
                    }
                    frame_ptr->data[FRAME_PAYLOAD_SIZE-1] = '\0';
                }
                else
                {
                    for(j=0; j < remainder; ++j)
                    {
                        frame_ptr->data[j] = message[j + i*(FRAME_PAYLOAD_SIZE-1)];
                    }
                    frame_ptr->data[j] = '\0'; 
                }
            }
            senderFrameConstructor(frame_ptr, position_marker, m_ip_addr, dest_ip);
            addMessageToPendingList(frame_ptr);
        }
    }
    else
    {
        Frame* outgoing_frame = new Frame;
        for(int i=0; i < message.length(); ++i)
        {
        	outgoing_frame->data[i] = message[i];
        }
        outgoing_frame->data[message.length()] = '\0';

        senderFrameConstructor(outgoing_frame, 4, m_ip_addr, dest_ip);
        addMessageToPendingList(outgoing_frame);
    }
}

void SenderHost::senderFrameConstructor(Frame* input_frame, uint8_t marker, uint32_t src_ip, uint32_t dest_ip)
{
    if(marker == 1)
    {
        input_frame->start_of_message = true;
        input_frame->end_of_message = false;
    }
    else if(marker == 2)
    {
        input_frame->start_of_message = false;
        input_frame->end_of_message = false;
    }
    else if(marker == 3)
    {
        input_frame->start_of_message = false;
        input_frame->end_of_message = true;
    }
    else
    {
        input_frame->start_of_message = true;
        input_frame->end_of_message = true;
    }
    input_frame->crc = 0;
    input_frame->src_ip = src_ip;
    input_frame->dest_ip = dest_ip;
    input_frame->seq_ack_num = m_sender_block_list[dest_ip]->SEQ;
    input_frame->is_it_ack = false;
    ++m_sender_block_list[dest_ip]->SEQ;
}

//Its job is to look into each frame in each pending frame list and then decide whether to put each packet in m_output_queue
void SenderHost::handlePendingFrames()
{
    for(auto it = m_sender_block_list.begin(); it != m_sender_block_list.end(); ++it)
    {
        SenderBlock* curr_block = it->second;
        if(curr_block->pending_frame_list.empty())
        {
            continue; //Nothing to do so continue
        }
       
        while(!curr_block->pending_frame_list.empty())
        {
            pair<uint32_t, uint8_t*> peek_elem = curr_block->pending_frame_list.front();
            
            Frame* peek_frame = (Frame*)(peek_elem.second + sizeof(EthernetHeader) + sizeof(IpHeader));
           
            uint8_t new_int = curr_block->LAR + curr_block->SWS;
            //Check if a sequence number is within the window
            if((curr_block->LAR < peek_frame->seq_ack_num && peek_frame->seq_ack_num <= new_int) || 
               ((new_int < curr_block->LAR) && (curr_block->LAR < peek_frame->seq_ack_num || peek_frame->seq_ack_num <= new_int)))   
            {
                curr_block->pending_frame_list.pop();
      
                curr_block->sender_queue[peek_frame->seq_ack_num].elem = peek_elem;

                struct timeval* next_time_out = &(curr_block->sender_queue[peek_frame->seq_ack_num].time_out);
                calculate_timeout(next_time_out);

                (curr_block->sender_queue[peek_frame->seq_ack_num]).ack_marked = false;

                curr_block->LFS = peek_frame->seq_ack_num;
                
                m_output_queue.push(peek_elem);
            }
            else
            {
                break;
            }
        }
    }
}

void SenderHost::handleTimeoutFrames()
{
    for(auto it = m_sender_block_list.begin(); it != m_sender_block_list.end(); ++it)
    {
        SenderBlock* curr_block = it->second;
        if(curr_block->LAR != curr_block->LFS)
        {
            uint8_t index = curr_block->LAR + 1;
            while(index != curr_block->LFS)
            {
                handleTimeoutHelper(curr_block, index);
                ++index;
            }
          	handleTimeoutHelper(curr_block, index); //Handle index = LFS
        }
	}
}

void SenderHost::handleTimeoutHelper(SenderBlock* curr_block, uint8_t index)
{
	struct timeval* start_time = &(curr_block->sender_queue[index].time_out);
    struct timeval* finish_time = (struct timeval*)malloc(sizeof(struct timeval));

    gettimeofday(finish_time, NULL);

    long time_diff = timeval_usecdiff(start_time, finish_time);
    if(0 <= time_diff)
    {
        struct timeval* time_out = &(curr_block->sender_queue[index].time_out);
        calculate_timeout(time_out);
        m_output_queue.push(curr_block->sender_queue[index].elem);
    }

    free(finish_time);
}

//Its job is to handle two kinds of messages, which are ACK and ICMP
void SenderHost::handleIncomingRouterMessage()
{
	if(m_input_queue.empty())
	{
		return;
	}
 	pthread_mutex_lock(&m_input_queue_mutex);
    while(!m_input_queue.empty())
    {
        pair<uint32_t, uint8_t*> elem = m_input_queue.front();
        m_input_queue.pop();
        EthernetHeader* ethernet_hdr = (EthernetHeader*)elem.second;
        IpHeader* ip_hdr = (IpHeader*)(elem.second + sizeof(EthernetHeader));

        if(ethernet_hdr->ether_type == ethertype_arp)
        {
        	handleArpRequestToMe(elem.second);
        	m_output_queue.push(elem);
        }
        else
        {
        	if(ip_hdr->ip_dst == m_ip_addr)
        	{	
				if(ip_hdr->ip_p == ip_protocol_icmp)
        		{
        			handleIcmpMessage(elem.second);
        		}
        		else //Ack
        		{
        			Frame* ack_frame = (Frame*)(elem.second + sizeof(EthernetHeader) + sizeof(IpHeader));
        
                	SenderBlock* curr_window = m_sender_block_list[ip_hdr->ip_src]; 

                	uint8_t expected_ack = curr_window->LAR + 1;
                	uint8_t acceptable_ack = curr_window->LFS;

                	if((expected_ack <= ack_frame->seq_ack_num && ack_frame->seq_ack_num <= acceptable_ack) ||
                   	(curr_window->LFS < curr_window->LAR && (expected_ack <= ack_frame->seq_ack_num || ack_frame->seq_ack_num <= acceptable_ack)))
                	{
                    	curr_window->sender_queue[ack_frame->seq_ack_num].ack_marked = true;
                    	if(expected_ack == ack_frame->seq_ack_num)
                    	{
                        	uint8_t index = ack_frame->seq_ack_num;
                        	while(curr_window->sender_queue[index].ack_marked)
                        	{
                            	curr_window->sender_queue[index++].ack_marked = false;
                            	++(curr_window->LAR);
                        	}
                    	}
                	}
            	}
        	}
        }
    }
    pthread_mutex_unlock(&m_input_queue_mutex);
}

void SenderHost::handleIcmpMessage(uint8_t* buf)
{
	IcmpT11Header* icmp_hdr = (IcmpT11Header*)(buf+ sizeof(EthernetHeader) + sizeof(IpHeader));
	if(icmp_hdr->icmp_type == icmp_dest_unreachable)
	{
		printf("Destination Unreachable! \n");
		unordered_map<uint32_t, SenderBlock*>::iterator it = m_sender_block_list.find(icmp_hdr->unused);
		if(it != m_sender_block_list.end())
		{
			m_sender_block_list.erase(icmp_hdr->unused);
		}
	}
}
















