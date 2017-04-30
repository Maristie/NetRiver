#include "sysInclude.h"

// set some parameters
#define max_tcb_num 10
#define wait_time	10000

// definition as instructions of NetRiver
enum state_kind {
	CLOSED, SYN_SENT, ESTABLISHED, FIN_WAIT1, FIN_WAIT2, TIME_WAIT
};

// structure of TCB
struct TCB {
	unsigned int srcAddr;
	unsigned short srcPort;
	unsigned int dstAddr;
	unsigned short dstPort;
	state_kind state;
	unsigned int seq;
	int socketnum;
};
TCB tcbList[max_tcb_num];
int cursor, record;
static int gSrcPort = 2005;
static int gDstPort = 2006;
static int gSeqNum = 0;
static int gAckNum = 0;
extern void tcp_DiscardPkt(char *pBuffer, int type);
extern void tcp_sendReport(int type);
extern void tcp_sendIpPkt(unsigned char *pData, UINT16 len, unsigned int
srcAddr, unsigned int dstAddr, UINT8 ttl);
extern int waitIpPacket(char *pBuffer, int timeout);
extern unsigned int getIpv4Address();
extern unsigned int getServerIpv4Address();

int stud_tcp_input(char *pBuffer, unsigned short len, unsigned int srcAddr,
unsigned int dstAddr) {
	// verify checksum
	unsigned int sum = 0;

	sum += (srcAddr >> 16) + (srcAddr & 0x0000ffff) + (dstAddr >> 16) +
	(dstAddr & 0x0000ffff) + 4 + len;
	int i;
	if (len % 2 == 0) {
		for (i = 0; i < len; i += 2)
			sum += ntohs(((unsigned short *)pBuffer)[i / 2]);
	} else {
		for (i = 0; i < len - 1; i += 2)
			sum += ntohs(((unsigned short *)pBuffer)[i / 2]);
		sum += (((unsigned short)(pBuffer[len - 1])) << 8);
	}
	if ((unsigned short)(~((sum >> 16) + (sum & 0x0000ffff))) != 0)
		return -1;
	// check state of finite-state machine
	switch (tcbList[cursor].state) {
	case SYN_SENT: {
		if (pBuffer[13] == 0x12) {
			if (tcbList[cursor].seq + 1 == ntohl(((unsigned
			int *)pBuffer)[2])) {
				stud_tcp_output(0, 0, PACKET_TYPE_ACK, tcbList[cursor].srcPort,
				tcbList[cursor].dstPort, srcAddr, dstAddr); // port is useless
				tcbList[cursor].state = ESTABLISHED;
				tcbList[cursor].seq = ntohl(((unsigned int *)pBuffer)[2]);
				break;
			} else {
				tcp_DiscardPkt(pBuffer, STUD_TCP_TEST_SEQNO_ERROR);
				return -1;
			}
		} else {
			return -1;
		}
	}
	case ESTABLISHED: {
		if (pBuffer[13] == 0x10) {
			if (tcbList[cursor].seq + len - 20 == ntohl(((unsigned
			int *)pBuffer)[2])) {
				tcbList[cursor].seq = ntohl(((unsigned int *)pBuffer)[2]);
				break;
			} else {
				tcp_DiscardPkt(pBuffer, STUD_TCP_TEST_SEQNO_ERROR);
				return -1;
			}
		} else {
			return -1;
		}
	}
	case FIN_WAIT1: {
		if (pBuffer[13] == 0x10) {
			if (tcbList[cursor].seq + 1 == ntohl(((unsigned
			int *)pBuffer)[2])) {
				tcbList[cursor].state = FIN_WAIT2;
				tcbList[cursor].seq = ntohl(((unsigned int *)pBuffer)[2]);
				break;
			} else {
				tcp_DiscardPkt(pBuffer, STUD_TCP_TEST_SEQNO_ERROR);
				return -1;
			}
		} else {
			return -1;
		}
	}
	case FIN_WAIT2: {
		if (pBuffer[13] == 0x11) {
			if (tcbList[cursor].seq == ntohl(((unsigned int *)pBuffer)[2])) {
				stud_tcp_output(0, 0, PACKET_TYPE_ACK, 0, 0, srcAddr, dstAddr); // port is useless
				tcbList[cursor].state = TIME_WAIT;
				break;
			} else {
				tcp_DiscardPkt(pBuffer, STUD_TCP_TEST_SEQNO_ERROR);
				return -1;
			}
		} else {
			return -1;
		}
	}
	}
	return 0;
}

void stud_tcp_output(char *pData, unsigned short len, unsigned char flag,
unsigned short srcPort, unsigned short dstPort, unsigned int srcAddr,
unsigned int dstAddr) {
	// set new TCP packet pointer
	char *pBuffer = (char *)malloc(20 + len);

	((unsigned short *)pBuffer)[0] = htons(srcPort);
	((unsigned short *)pBuffer)[1] = htons(dstPort);
	((unsigned int *)pBuffer)[1] = htonl(srcAddr);
	((unsigned int *)pBuffer)[2] = htonl(dstAddr);
	pBuffer[12] = 5 << 4;
	((unsigned short *)pBuffer)[7] = htons((unsigned short)1);
	((unsigned short *)pBuffer)[9] = 0;
	memcpy(pBuffer + 20, pData, len);
	// check finite-state machine
	switch (flag) {
	case PACKET_TYPE_SYN: {
		pBuffer[13] = 0x02;
		tcbList[cursor].state = SYN_SENT;
		tcbList[cursor].seq = gSeqNum;
		tcbList[cursor].srcAddr = srcAddr;
		tcbList[cursor].srcPort = srcPort;
		tcbList[cursor].dstAddr = dstAddr;
		tcbList[cursor].dstPort = dstPort;
		break;
	}
	case PACKET_TYPE_FIN_ACK: {
		pBuffer[13] = 0x11;
		tcbList[cursor].state = FIN_WAIT1;
		break;
	}
	case PACKET_TYPE_ACK: {
		pBuffer[13] = 0x10;
		break;
	}
	}
	// set checksum
	((unsigned short *)pBuffer)[8] = 0;
	unsigned int sum = 0;
	sum += (srcAddr >> 16) + (srcAddr & 0x0000ffff) + (dstAddr >> 16) +
	(dstAddr & 0x0000ffff) + 24 + len;
	int i;
	if (len % 2 == 0) {
		for (i = 0; i < len; i += 2)
			sum += ntohs(((unsigned short *)pBuffer)[i / 2]);
	} else {
		for (i = 0; i < len - 1; i += 2)
			sum += ntohs(((unsigned short *)pBuffer)[i / 2]);
		sum += ((unsigned short)(pBuffer[len - 1])) << 8;
	}
	((unsigned short *)pBuffer)[8] = htons((unsigned short)(~((sum >> 16) +
	(sum & 0x0000ffff))));
	tcp_sendIpPkt((unsigned char *)pBuffer, len + 20, srcAddr, dstAddr, 100);
}

// initialize tcb
int stud_tcp_socket(int domain, int type, int protocol) {
	tcbList[cursor].state = CLOSED;
	tcbList[cursor].seq = gSeqNum;
	tcbList[cursor].socketnum = record;
	return tcbList[cursor].socketnum;
}

int stud_tcp_connect(int sockfd, struct sockaddr_in *addr, int addrlen) {
	// find tcb matching sockfd
	int i;

	for (int i = 0; i < max_tcb_num; ++i)
		if (tcbList[i].socketnum == sockfd) {
			cursor = i;
			break;
		}
	// set tcb properties
	tcbList[cursor].srcAddr = getIpv4Address();
	tcbList[cursor].dstAddr = getServerIpv4Address();
	tcbList[cursor].srcPort = gSrcPort;
	tcbList[cursor].dstPort = gDstPort;
	tcbList[cursor].state = SYN_SENT;
	// establish connection with server
	stud_tcp_output(0, 0, PACKET_TYPE_SYN, tcbList[cursor].srcPort,
	tcbList[cursor].dstPort, tcbList[cursor].srcAddr,
	tcbList[cursor].dstAddr);
	char *pBuffer = (char *)malloc(20);
	while (waitIpPacket(pBuffer, wait_time)) {}
	stud_tcp_input(pBuffer, 20, tcbList[cursor].srcAddr,
	tcbList[cursor].dstAddr);
	return 0;
}

int stud_tcp_send(int sockfd, const unsigned char *pData, unsigned short
datalen, int flags) {
	// find tcb matching sockfd
	int i;

	for (int i = 0; i < max_tcb_num; ++i)
		if (tcbList[i].socketnum == sockfd) {
			cursor = i;
			break;
		}
	// send data and wait for ack
	stud_tcp_output((char *)pData, datalen, PACKET_TYPE_DATA,
	tcbList[cursor].srcPort, tcbList[cursor].dstPort,
	tcbList[cursor].srcAddr, tcbList[cursor].dstAddr);
	char *pBuffer = (char *)malloc(20);
	while (waitIpPacket(pBuffer, wait_time)) {}
	stud_tcp_input(pBuffer, 20, tcbList[cursor].srcAddr,
	tcbList[cursor].dstAddr);
	return 0;
}

int stud_tcp_recv(int sockfd, unsigned char *pData, unsigned short datalen, int
flags) {
	// find tcb matching sockfd
	int i;

	for (int i = 0; i < max_tcb_num; ++i)
		if (tcbList[i].socketnum == sockfd) {
			cursor = i;
			break;
		}
	// receive data and send ack
	char *pBuffer = (char *)malloc(20);
	while (waitIpPacket(pBuffer, wait_time)) {}
	stud_tcp_output(0, 0, PACKET_TYPE_ACK, tcbList[cursor].srcPort,
	tcbList[cursor].dstPort, tcbList[cursor].srcAddr,
	tcbList[cursor].dstAddr);
	return 0;
}

int stud_tcp_close(int sockfd) {
	// find tcb matching sockfd
	int i;

	for (int i = 0; i < max_tcb_num; ++i)
		if (tcbList[i].socketnum == sockfd) {
			cursor = i;
			break;
		}
	// disconnect with server by sending fin, receiving ack with fin from server then finally sending ack
	stud_tcp_output(0, 0, PACKET_TYPE_FIN_ACK, tcbList[cursor].srcPort,
	tcbList[cursor].dstPort, tcbList[cursor].srcAddr,
	tcbList[cursor].dstAddr);
	tcbList[cursor].state = FIN_WAIT1;
	char *pBuffer = (char *)malloc(20);
	while (waitIpPacket(pBuffer, wait_time)) {}
	stud_tcp_input(pBuffer, 20, tcbList[cursor].srcAddr,
	tcbList[cursor].dstAddr);
	while (waitIpPacket(pBuffer, wait_time)) {}
	stud_tcp_input(pBuffer, 20, tcbList[cursor].srcAddr,
	tcbList[cursor].dstAddr);
	return 0;
}
