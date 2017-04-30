#include "sysinclude.h"

extern void ipv6_DiscardPkt(char *pBuffer, int type);
extern void ipv6_SendtoLower(char *pBuffer, int length);
extern void ipv6_SendtoUp(char *pBuffer, int length);
extern void getIpv6Address(ipv6_addr *paddr);

int stud_ipv6_recv(char *pBuffer, unsigned short length) {
	// check version
	if ((pBuffer[0] >> 4) != 6) {
		ipv6_DiscardPkt(pBuffer, STUD_IPV6_TEST_VERSION_ERROR);
		return -1;
	}
	// check hop limit
	if (pBuffer[7] == 0) {
		ipv6_DiscardPkt(pBuffer, STUD_IPV6_TEST_HOPLIMIT_ERROR);
		return -1;
	}

	// check destination
	// get destination address
	BYTE des_addr[16];
	memcpy(des_addr, pBuffer + 24, 16);
	// get local address
	ipv6_addr *local_addr = (ipv6_addr *)malloc(sizeof (ipv6_addr));
	getIpv6Address(local_addr);
	// compare destination and local address
	int flag = 0;
	for (int i = 0; i < 16; ++i)
		if (des_addr[i] != local_addr->bAddr[i]) {
			flag = 1;
			break;
		}
	// if neither local host nor multicast
	if (flag == 1 && des_addr[0] != 0xff) {
		ipv6_DiscardPkt(pBuffer, STUD_IPV6_TEST_DESTINATION_ERROR);
		return -1;
	}
	// no problem, and send to upper layer
	ipv6_SendtoUp(pBuffer, length);
	return 0;
}

int stud_ipv6_Upsend(char *pData, unsigned short len, ipv6_addr *srcAddr,
	ipv6_addr *dstAddr, char hoplimit, char nexthead) {
	// allocate space for buffer and copy payload
	char *lower_send_buffer = (char *)malloc(sizeof (char) * (len + 40));

	memcpy(lower_send_buffer + 40, pData, len);

	// set version
	lower_send_buffer[0] = 6 << 4;

	// set traffice class (this part crosses 2 bytes)
	lower_send_buffer[1] = 0;

	/*
	 * set flow label (this part crosses 3 bytes), now it's experimental and
	 * for hosts or routers who don't support it just set it to 0
	 */
	((unsigned short *)lower_send_buffer)[1] = 0;

	// set payload length
	((unsigned short *)lower_send_buffer)[2] = htons(len);

	// set next header
	((char *)lower_send_buffer)[6] = nexthead;

	// set hop limit
	((char *)lower_send_buffer)[7] = hoplimit;

	// set source address and destination address
	memcpy(lower_send_buffer + 8, srcAddr->bAddr, 16);
	memcpy(lower_send_buffer + 24, dstAddr->bAddr, 16);

	// send to lower layer
	ipv6_SendtoLower(lower_send_buffer, len + 40);

	return 0;
}
