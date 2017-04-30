#include "sysInclude.h"

extern void fwd_LocalRcv(char *pBuffer, int length);
extern void fwd_SendtoLower(char *pBuffer, int length, unsigned int nexthop);
extern void fwd_DiscardPkt(char *pBuffer, int type);
extern unsigned int getIpv4Address();
static stud_route_msg route_table[50];
static int table_size = 0;

void stud_Route_Init() {}

void stud_route_add(stud_route_msg *proute) {
	route_table[table_size++] = *proute;
}

int stud_fwd_deal(char *pBuffer, int length) {
	// get destination address
	unsigned int des_addr = ntohl(((unsigned int *)pBuffer)[4]);

	// check TTL
	if (pBuffer[8] == 0) {
		ip_DiscardPkt(pBuffer, STUD_FORWARD_TEST_TTLERROR);
		return 1;
	}
	// if destination is local host
	if (des_addr == getIpv4Address()) {
		fwd_LocalRcv(pBuffer, length);
		return 0;
	}

	// not local host, then search route table
	// max_masklen is the maximum of masklen of route table items who've been successfully matched, and nexthop is to record
	unsigned int max_masklen = 0, nexthop;
	for (int i = 0; i < table_size; ++i) {
		unsigned int masklen = ntohl(route_table[i].masklen);
		// compare mask address, that is, compare prefixes whose length is masklen
		if (ntohl(route_table[i].dest) >> (32 - masklen) == des_addr >> (32 -
		masklen) && masklen > max_masklen) {
			max_masklen = masklen;
			nexthop = ntohl(route_table[i].nexthop);
		}
	}
	// if no route found
	if (max_masklen == 0) {
		fwd_DiscardPkt(pBuffer, STUD_FORWARD_TEST_NOROUTE);
		return 1;
	}
	// route found, then prepare for sending
	// set TTL
	--pBuffer[8];

	// set header checksum
	((unsigned short *)pBuffer)[5] = 0;
	unsigned int sum = 0;
	for (int i = 0; i < 10; ++i)
		sum += ntohs(((unsigned short *)pBuffer)[i]);
	((unsigned short *)pBuffer)[5] = htons((unsigned short)(~((sum >> 16) +
	(sum & 0x0000ffff))));

	// finally send
	fwd_SendtoLower(pBuffer, length, nexthop);

	return 0;
}
