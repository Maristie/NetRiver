#include "sysInclude.h"

extern void ip_DiscardPkt(char *pBuffer, int type);
extern void ip_SendtoLower(char *pBuffer, int length);
extern void ip_SendtoUp(char *pBuffer, int length);
extern unsigned int getIpv4Address();

int stud_ip_recv(char *pBuffer, unsigned short length) {
	// check version
	if ((pBuffer[0] >> 4) != 4) {
		ip_DiscardPkt(pBuffer, STUD_IP_TEST_VERSION_ERROR);
		return 1;
	}

	// check ip head length
	int len = pBuffer[0] & 0x0f;
	if (len < 5) {
		ip_DiscardPkt(pBuffer, STUD_IP_TEST_HEADLEN_ERROR);
		return 1;
	}
	// check time to live
	if (pBuffer[8] == 0) {
		ip_DiscardPkt(pBuffer, STUD_IP_TEST_TTL_ERROR);
		return 1;
	}

	// check header checksum
	unsigned int sum = 0;
	for (int i = 0; i < len * 2; ++i)
		sum += ntohs(((unsigned short *)pBuffer)[i]);
	if ((unsigned short)(~((sum >> 16) + (sum & 0x0000ffff))) != 0) {
		ip_DiscardPkt(pBuffer, STUD_IP_TEST_CHECKSUM_ERROR);
		return 1;
	}

	// check destination address, broadcast IP is in form of x.x.x.255 since it's an IP of C class
	unsigned int addr = ntohl(((unsigned int *)pBuffer)[4]);
	if (addr != getIpv4Address() && (addr & 0x000000ff) != 255) {
		ip_DiscardPkt(pBuffer, STUD_IP_TEST_DESTINATION_ERROR);
		return 1;
	}
	// all checks passed, then send to upper layer
	// needn't protocol type of upper layer
	ip_SendtoUp(pBuffer, length);
	return 0;
}

int stud_ip_Upsend(char *pBuffer, unsigned short len, unsigned int srcAddr,
	unsigned int dstAddr, byte protocol, byte ttl) {
	// allocate space
	char *lower_send_buffer = (char *)malloc(sizeof (char) * (len + 20));

	memcpy(lower_send_buffer + 20, pBuffer, len);

	// set version and ip head length
	lower_send_buffer[0] = (4 << 4) + 5;

	// set Type of service
	lower_send_buffer[2] = 0;

	// set total length
	((unsigned short *)lower_send_buffer)[1] = htons(len + 20);

	// set identification
	((unsigned short *)lower_send_buffer)[2] = htons(rand() % 65536);

	// set DF, MF and fragment offset
	((unsigned short *)lower_send_buffer)[3] = htons((1 << 6) + (0 << 5) + 0);

	// set time to live
	((byte *)lower_send_buffer)[8] = ttl;

	// set protocol
	((byte *)lower_send_buffer)[9] = protocol;

	// set source address and destination address
	((unsigned int *)lower_send_buffer)[3] = htonl(srcAddr);
	((unsigned int *)lower_send_buffer)[4] = htonl(dstAddr);

	// finally set header checksum
	((unsigned short *)lower_send_buffer)[5] = 0;
	unsigned int sum = 0;
	for (int i = 0; i < 10; ++i)
		sum += ntohs(((unsigned short *)lower_send_buffer)[i]);
	((unsigned short *)lower_send_buffer)[5] = htons((unsigned short)(~((sum >>
	16) + (sum & 0x0000ffff))));

	// send to lower layer
	ip_SendtoLower(lower_send_buffer, len + 20);

	return 0;
}
