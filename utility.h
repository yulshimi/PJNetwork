#ifndef __UTILITY_H__
#define __UTILITY_H__

#define ETHER_ADDR_LEN 6
#define ICMP_DATA_SIZE 28
#include <stdint.h>
#include <vector>
#include <string>
#include <sys/time.h>
using namespace std;
struct EthernetHeader
{
    uint8_t  ether_dhost[ETHER_ADDR_LEN];    /* destination ethernet address */
    uint8_t  ether_shost[ETHER_ADDR_LEN];    /* source ethernet address */
    uint16_t ether_type;                     /* packet type ID */
} __attribute__ ((packed));
typedef struct EthernetHeader EthernetHeader;

struct IpHeader
{
    unsigned int ip_v;		/* version */
    unsigned int ip_hl;		/* header length */
    uint8_t ip_tos;			/* type of service */
    uint16_t ip_len;			/* total length */
    uint16_t ip_id;			/* identification */
    uint16_t ip_off;			/* fragment offset field */
#define	IP_RF 0x8000			/* reserved fragment flag */
#define	IP_DF 0x4000			/* dont fragment flag */
#define	IP_MF 0x2000			/* more fragments flag */
#define	IP_OFFMASK 0x1fff		/* mask for fragmenting bits */
    uint8_t ip_ttl;			/* time to live */
    uint8_t ip_p;			/* protocol */
    uint16_t ip_sum;			/* checksum */
    uint32_t ip_src, ip_dst;	/* source and dest address */
} __attribute__ ((packed)) ;
typedef struct IpHeader IpHeader;

struct ArpHeader
{
    unsigned short  ar_hrd;             /* format of hardware address   */
    unsigned short  ar_pro;             /* format of protocol address   */
    unsigned char   ar_hln;             /* length of hardware address   */
    unsigned char   ar_pln;             /* length of protocol address   */
    unsigned short  ar_op;              /* ARP opcode (command)         */
    unsigned char   ar_sha[ETHER_ADDR_LEN];   /* sender hardware address      */
    uint32_t        ar_sip;             /* sender IP address            */
    unsigned char   ar_tha[ETHER_ADDR_LEN];   /* target hardware address      */
    uint32_t        ar_tip;             /* target IP address            */
} __attribute__ ((packed)) ;
typedef struct ArpHeader ArpHeader;

#define FRAME_PAYLOAD_SIZE 20
struct Frame
{
    uint32_t src_ip; //2 Bytes
    uint32_t dest_ip; //2 Bytes
    uint8_t seq_ack_num; //1 Byte
    bool is_it_ack; //1 Byte
    bool start_of_message; //1 Byte
    bool end_of_message; //1 Byte
    uint8_t data[FRAME_PAYLOAD_SIZE];
    uint8_t crc;
};
typedef struct Frame Frame;

struct FrameTime
{
    struct timeval time_out;
    bool ack_marked;
    pair<uint32_t, uint8_t*> elem; 
};
typedef struct FrameTime FrameTime;

struct IcmpT11Header
{
    uint8_t icmp_type;
    uint8_t icmp_code;
    uint16_t icmp_sum;
    uint32_t unused;
    uint8_t data[ICMP_DATA_SIZE];
} __attribute__ ((packed)) ;
typedef struct IcmpT11Header IcmpT11Header;

enum sr_ip_protocol 
{
	ip_protocol_tcp = 0x0000,
  	ip_protocol_icmp = 0x0001
};

enum sr_ethertype 
{
  ethertype_arp = 0x0806,
  ethertype_ip = 0x0800
};

enum sr_arp_opcode 
{
  arp_op_request = 0x0001,
  arp_op_reply = 0x0002
};

enum sr_arp_hrd_fmt 
{
  arp_hrd_ethernet = 0x0001,
};

enum icmp_error_type
{
	icmp_dest_unreachable = 3
};

uint16_t cksum(void *_data, int len);
void calculate_timeout(struct timeval* timeout);
long timeval_usecdiff(struct timeval* start_time, struct timeval* finish_time);
vector<string> splice(string input, char split_char);
uint32_t convertTo32Bits(vector<uint32_t> in_vector);
vector<uint32_t> fromStrToUintVector(vector<string> str_vector);
#endif
