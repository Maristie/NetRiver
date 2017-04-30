#include "sysinclude.h"

extern void ipv6_fwd_DiscardPkt(char *pBuffer, int type);
extern void ipv6_fwd_SendtoLower(char *pBuffer, int length, ipv6_addr *nexthop);
extern void getIpv6Address(ipv6_addr *pAddr);
extern void ipv6_fwd_LocalRcv(char *pBuffer, int length);
static stud_ipv6_route_msg route_table[50];
static int table_size = 0;

void stud_ipv6_Route_Init() {}

void stud_ipv6_route_add(stud_ipv6_route_msg *proute) {
	route_table[table_size++] = *proute;
}

int stud_ipv6_fwd_deal(char *pBuffer, int length) {
	// get destination address
	BYTE des_addr[16];

	memcpy(des_addr, pBuffer + 24, 16);

	// counters declaration
	int i, j;

	// check hoplimit
	if (pBuffer[7] == 0) {
		ipv6_fwd_DiscardPkt(pBuffer, STUD_IPV6_FORWARD_TEST_HOPLIMIT_ERROR);
		return 1;
	}

	// check destination
	// get local address
	ipv6_addr *local_addr = (ipv6_addr *)malloc(sizeof (ipv6_addr));
	getIpv6Address(local_addr);
	// compare destination and local address
	int flag = 0;
	for (i = 0; i < 16; ++i)
		if (des_addr[i] != local_addr->bAddr[i]) {
			flag = 1;
			break;
		}
	if (flag == 0) {
		ipv6_fwd_LocalRcv(pBuffer, length);
		return 0;
	}

	// not local host, then search route table
	unsigned int max_masklen = 0;
	ipv6_addr *nexthop = (ipv6_addr *)malloc(sizeof (ipv6_addr));   // used to record the next hop
	// traverse the route table
	for (i = 0; i < table_size; ++i) {
		unsigned int masklen = route_table[i].masklen;
		flag = 0;                       // flag == 0 means they have the same prefix, or else they're different
		for (j = 0; j < masklen / 8; ++j)
			if (des_addr[j] != route_table[i].dest.bAddr[j]) {
				flag = 1;
				break;
			}
		if (flag == 0 && des_addr[j] >> ((j + 1) * 8 - masklen) !=
		route_table[i].dest.bAddr[j] >> ((j + 1) * 8 - masklen))
			flag = 1;
		// judge flag
		if (flag == 0 && masklen > max_masklen) {
			max_masklen = masklen;
			*nexthop = route_table[i].nexthop;
		}
	}
	// if no route found
	if (max_masklen == 0) {
		fwd_DiscardPkt(pBuffer, STUD_IPV6_FORWARD_TEST_NOROUTE);
		return 1;
	}
	// route found, then prepare for sending
	// set hoplimit
	--pBuffer[7];
	// send
	ipv6_fwd_SendtoLower(pBuffer, length, nexthop);

	return 0;
}
