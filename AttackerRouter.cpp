#include "router.h"

AttackerRouter::AttackerRouter(ifstream& in_file, uint32_t r_id):Router(in_file, r_id){}

bool AttackerRouter::attackObject(uint8_t* buf)
{
	IpHeader* ip_hdr = (IpHeader*)(buf + sizeof(EthernetHeader));
	unordered_map<uint32_t, uint8_t>::iterator it = m_hack_ack_data.find(ip_hdr->ip_src);
	if(it != m_hack_ack_data.end())
	{
		return false;
	}
	Frame* frame = (Frame*)(buf + sizeof(EthernetHeader) + sizeof(IpHeader));
	return (frame->end_of_message == true) && (frame->is_it_ack == false); //This message should be attacked
}

bool AttackerRouter::alterSeqAckNum(uint8_t* buf)
{
	Frame* frame = (Frame*)(buf + sizeof(EthernetHeader) + sizeof(IpHeader));
	IpHeader* ip_hdr = (IpHeader*)(buf + sizeof(EthernetHeader));
	uint32_t target_ip;
	if(frame->is_it_ack)
	{
		target_ip = ip_hdr->ip_dst;
	}
	else
	{
		target_ip = ip_hdr->ip_src;
	}
	unordered_map<uint32_t, bool>::iterator it = m_seq_ack_control_flag.find(target_ip);
	if(it != m_seq_ack_control_flag.end())
	{
		unordered_map<uint32_t, uint8_t>::iterator got = m_hack_ack_data.find(target_ip);
		assert(got != m_hack_ack_data.end());
		if(got->second < frame->seq_ack_num)
		{
			if(frame->is_it_ack)
			{
				frame->seq_ack_num -= 1;
			}
			else
			{
				frame->seq_ack_num += 1;
			}
		}
		return true;
	}
	return false;
}

bool AttackerRouter::dropAck(uint8_t* buf)
{
	Frame* frame = (Frame*)(buf + sizeof(EthernetHeader) + sizeof(IpHeader));
	IpHeader* ip_hdr = (IpHeader*)(buf + sizeof(EthernetHeader));
	unordered_map<uint32_t, uint8_t>::iterator it = m_hack_ack_data.find(ip_hdr->ip_dst);
	if(it != m_hack_ack_data.end() && it->second+1 == frame->seq_ack_num && frame->is_it_ack == true)
	{
		m_seq_ack_control_flag.insert(make_pair(ip_hdr->ip_dst, true));
		return true;
	}
	return false;
}


void AttackerRouter::createAttackMessage(uint8_t* buf_1, uint8_t* buf_2)
{
	//printf("This message is going to be attacked \n");
	memcpy(buf_2, buf_1, sizeof(EthernetHeader) + sizeof(IpHeader) + sizeof(Frame));
	IpHeader* ip_hdr = (IpHeader*)(buf_1 + sizeof(EthernetHeader));
	Frame* frame_buf_1 = (Frame*)(buf_1 + sizeof(EthernetHeader) + sizeof(IpHeader));
	frame_buf_1->end_of_message = false;
	
	Frame* frame_buf_2 = (Frame*)(buf_2 + sizeof(EthernetHeader) + sizeof(IpHeader));
	strncpy((char*)frame_buf_2->data, "Message Attacked!", FRAME_PAYLOAD_SIZE);
	frame_buf_2->seq_ack_num = frame_buf_1->seq_ack_num + 1;
	frame_buf_2->start_of_message = false;
	frame_buf_2->end_of_message = true;
	m_hack_ack_data.insert(make_pair(ip_hdr->ip_src, frame_buf_1->seq_ack_num));
}

void AttackerRouter::attackerHandlePacket(uint8_t* packet, uint32_t packet_len, string interface)
{
	assert(packet);
    assert(0 < interface.length());

    printf("*** -> Received packet of length %d \n", packet_len);
    //cout << "I am router " << m_id << endl;
    cout << "Attacker Router \n";
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

        if(cksum(ip_ptr, sizeof(IpHeader)) != temp_sum)
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
                	uint8_t* new_packet = 0;
                	if(attackObject(packet))
                	{
                		new_packet = (uint8_t*)malloc(sizeof(EthernetHeader) + sizeof(IpHeader) + sizeof(Frame));
                		printf("About to create Attack Message \n");
                		createAttackMessage(packet, new_packet);
                	}
                	else
                	{
                		if(dropAck(packet))
                		{
                			return;
                		}
                		else
                		{
                			alterSeqAckNum(packet);
                		}
                	}
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
                        if(new_packet)
                        {
                        	//printf("new_packet not null \n");
                        	IpHeader* ip_hdr = (IpHeader*)(new_packet + sizeof(EthernetHeader));
                        	--(ip_hdr->ip_ttl);

                        	ip_hdr->ip_sum = 0;
                        	ip_hdr->ip_sum = cksum(ip_hdr, sizeof(IpHeader));
                        	addPacketToOutputQueue(new_packet, packet_len, if_elem->name);
                        }
                    }
                    else // cache miss
                    {
                    	//printf("Cache Miss \n");
                        arpCacheQueueReq(addr_box->next_hop, packet, packet_len, addr_box->interface_name);
                        if(new_packet)
                        {
                        	arpCacheQueueReq(addr_box->next_hop, new_packet, packet_len, addr_box->interface_name);
                        }
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