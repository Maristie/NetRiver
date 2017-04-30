#include "sysinclude.h"

extern void SendFRAMEPacket(unsigned char *pdata, unsigned int len);

#define WINDOW_SIZE_STOP_WAIT		1
#define WINDOW_SIZE_BACK_N_FRAME	4
#define max_wait_num				1000

/*
 * The following definitions are just as NetRiver instructions.
 */
typedef enum { data, ack, nak } frame_kind;
typedef struct frame_head {
	frame_kind kind;
	unsigned int seq;
	unsigned int ack;
	unsigned char data[100];
};
typedef struct frame {
	frame_head head;
	unsigned int size;
};

/*
 * stop and wait
 */
int stud_slide_window_stop_and_wait(char *pBuffer, int bufferSize, UINT8
	messageType) {
	// two queues for window and waiting
	static frame *window[WINDOW_SIZE_STOP_WAIT] = { 0 };
	static frame *waiting[max_wait_num] = { 0 };
	static int win_begin = 0, win_end = 0, wait_begin = 0, wait_end = 0;
	static int win_num = 0, wait_num = 0;

	// when send
	if (messageType == MSG_TYPE_SEND) {
		// push pBuffer into waiting queue

		// allocate space for frame
		waiting[wait_end] = (frame *)malloc(sizeof (frame));
		// copy pBuffer to waiting queue
		memcpy(waiting[wait_end], (frame *)pBuffer, sizeof (frame));

		waiting[wait_end]->size = (unsigned int)bufferSize;
		wait_end = (wait_end + 1) % max_wait_num;
		wait_num++;
		// when window is not full
		if (win_num < WINDOW_SIZE_STOP_WAIT) {
			// pop one frame from waiting queue
			frame *temp = waiting[wait_begin];
			wait_begin = (wait_begin + 1) % max_wait_num;
			wait_num--;

			// push the frame into window
			window[win_end] = temp;
			win_end = (win_end + 1) % WINDOW_SIZE_STOP_WAIT;
			win_num++;

			// try to send
			SendFRAMEPacket((unsigned char *)temp, temp->size);
		}
		// when window is full, no more needs to be done
	}
	// when receive
	else if (messageType == MSG_TYPE_RECEIVE) {
		frame *temp = (frame *)pBuffer;
		frame_kind kind = (frame_kind)ntohl((unsigned long)temp->head.kind);

		// check kind
		if (kind == nak) {
			// when nak, first find which frame needs to be resent
			int cursor = win_begin;
			unsigned int seq = ntohl(temp->head.ack);
			while (seq != ntohl(window[cursor]->head.seq))
				cursor = (cursor + 1) % WINDOW_SIZE_STOP_WAIT;
			// then resend it
			SendFRAMEPacket((unsigned char *)window[cursor],
			window[cursor]->size);
		} else if (kind == ack) {
			// when ack, close windows before or equal it
			unsigned int ack = ntohl(temp->head.ack);
			while (ntohl(window[win_begin]->head.seq) <= ack) {
				// close a window
				free(window[win_begin]);
				win_begin = (win_begin + 1) % WINDOW_SIZE_STOP_WAIT;
				win_num--;
				// when waiting queue is not empty, then pop one and push into window
				if (wait_num > 0) {
					window[win_end] = waiting[wait_begin];

					// try to send it
					SendFRAMEPacket((unsigned char *)window[win_end],
					window[win_end]->size);

					wait_begin = (wait_begin + 1) % max_wait_num;
					wait_num--;
					win_end = (win_end + 1) % WINDOW_SIZE_STOP_WAIT;
					win_num++;
				}
			}
		}
	}
	// when time out
	else if (messageType == MSG_TYPE_TIMEOUT) {
		UINT32 temp = ntohl(*((UINT32 *)pBuffer));
		int cursor = win_begin;

		// resend all frames after MSG_TYPE_TIMEOUT
		if (temp <= window[cursor]->head.seq)
			SendFRAMEPacket((unsigned char *)window[cursor],
			window[cursor]->size);
		cursor = (cursor + 1) % WINDOW_SIZE_STOP_WAIT;
		while (cursor != win_end) {
			if (temp <= window[cursor]->head.seq)
				SendFRAMEPacket((unsigned char *)window[cursor],
				window[cursor]->size);
			cursor = (cursor + 1) % WINDOW_SIZE_STOP_WAIT;
		}
	}
	return 0;
}

/*
 * Almost same as stop-wait except WINDOW_SIZE_BACK_N_FRAME.
 */

/*
 * back n frame
 */
int stud_slide_window_back_n_frame(char *pBuffer, int bufferSize, UINT8
	messageType) {
	// two queues for window and waiting
	static frame *window[WINDOW_SIZE_BACK_N_FRAME] = { 0 };
	static frame *waiting[max_wait_num] = { 0 };
	static int win_begin = 0, win_end = 0, wait_begin = 0, wait_end = 0;
	static int win_num = 0, wait_num = 0;

	// when send
	if (messageType == MSG_TYPE_SEND) {
		// push pBuffer into waiting queue

		// allocate space for frame
		waiting[wait_end] = (frame *)malloc(sizeof (frame));
		// copy pBuffer to waiting queue
		memcpy(waiting[wait_end], (frame *)pBuffer, sizeof (frame));

		waiting[wait_end]->size = (unsigned int)bufferSize;
		wait_end = (wait_end + 1) % max_wait_num;
		wait_num++;
		// when window is not full
		if (win_num < WINDOW_SIZE_BACK_N_FRAME) {
			// pop one frame from waiting queue
			frame *temp = waiting[wait_begin];
			wait_begin = (wait_begin + 1) % max_wait_num;
			wait_num--;

			// push the frame into window
			window[win_end] = temp;
			win_end = (win_end + 1) % WINDOW_SIZE_BACK_N_FRAME;
			win_num++;

			// try to send
			SendFRAMEPacket((unsigned char *)temp, temp->size);
		}
		// when window is full, no more needs to be done
	}
	// when receive
	else if (messageType == MSG_TYPE_RECEIVE) {
		frame *temp = (frame *)pBuffer;
		frame_kind kind = (frame_kind)ntohl((unsigned long)temp->head.kind);

		// check kind
		if (kind == nak) {
			// when nak, first find which frame needs to be resent
			int cursor = win_begin;
			unsigned int seq = ntohl(temp->head.ack);
			while (seq != ntohl(window[cursor]->head.seq))
				cursor = (cursor + 1) % WINDOW_SIZE_BACK_N_FRAME;
			// then resend it
			SendFRAMEPacket((unsigned char *)window[cursor],
			window[cursor]->size);
		} else if (kind == ack) {
			// when ack, close windows before or equal it
			unsigned int ack = ntohl(temp->head.ack);
			while (ntohl(window[win_begin]->head.seq) <= ack) {
				// close a window
				free(window[win_begin]);
				win_begin = (win_begin + 1) % WINDOW_SIZE_BACK_N_FRAME;
				win_num--;
				// when waiting queue is not empty, then pop one and push into window
				if (wait_num > 0) {
					window[win_end] = waiting[wait_begin];

					// try to send it
					SendFRAMEPacket((unsigned char *)window[win_end],
					window[win_end]->size);

					wait_begin = (wait_begin + 1) % max_wait_num;
					wait_num--;
					win_end = (win_end + 1) % WINDOW_SIZE_BACK_N_FRAME;
					win_num++;
				}
			}
		}
	}
	// when time out
	else if (messageType == MSG_TYPE_TIMEOUT) {
		UINT32 temp = ntohl(*((UINT32 *)pBuffer));
		int cursor = win_begin;

		// resend all frames after MSG_TYPE_TIMEOUT
		// here is where I found the bug of lab system
		SendFRAMEPacket((unsigned char *)window[cursor], window[cursor]->size);
		cursor = (cursor + 1) % WINDOW_SIZE_BACK_N_FRAME;
		while (cursor != win_end) {
			SendFRAMEPacket((unsigned char *)window[cursor],
			window[cursor]->size);
			cursor = (cursor + 1) % WINDOW_SIZE_BACK_N_FRAME;
		}
	}
	return 0;
}

/*
 * Almost same as back-n-frame.
 * The only difference is that back-n-frame has TIMEOUT, but choice-frame-resend has nak.
 */

/*
 * choice frame resend
 */
int stud_slide_window_choice_frame_resend(char *pBuffer, int bufferSize, UINT8
	messageType) {
	// two queues for window and waiting
	static frame *window[WINDOW_SIZE_BACK_N_FRAME] = { 0 };
	static frame *waiting[max_wait_num] = { 0 };
	static int win_begin = 0, win_end = 0, wait_begin = 0, wait_end = 0;
	static int win_num = 0, wait_num = 0;

	// when send
	if (messageType == MSG_TYPE_SEND) {
		// push pBuffer into waiting queue

		// allocate space for frame
		waiting[wait_end] = (frame *)malloc(sizeof (frame));
		// copy pBuffer to waiting queue
		memcpy(waiting[wait_end], (frame *)pBuffer, sizeof (frame));

		waiting[wait_end]->size = (unsigned int)bufferSize;
		wait_end = (wait_end + 1) % max_wait_num;
		wait_num++;
		// when window is not full
		if (win_num < WINDOW_SIZE_BACK_N_FRAME) {
			// pop one frame from waiting queue
			frame *temp = waiting[wait_begin];
			wait_begin = (wait_begin + 1) % max_wait_num;
			wait_num--;

			// push the frame into window
			window[win_end] = temp;
			win_end = (win_end + 1) % WINDOW_SIZE_BACK_N_FRAME;
			win_num++;

			// try to send
			SendFRAMEPacket((unsigned char *)temp, temp->size);
		}
		// when window is full, no more needs to be done
	}
	// when receive
	else if (messageType == MSG_TYPE_RECEIVE) {
		frame *temp = (frame *)pBuffer;
		frame_kind kind = (frame_kind)ntohl((unsigned long)temp->head.kind);

		// check kind
		if (kind == nak) {
			// when nak, first find which frame needs to be resent
			int cursor = win_begin;
			unsigned int seq = ntohl(temp->head.ack);
			while (seq != ntohl(window[cursor]->head.seq))
				cursor = (cursor + 1) % WINDOW_SIZE_BACK_N_FRAME;
			// then resend it
			SendFRAMEPacket((unsigned char *)window[cursor],
			window[cursor]->size);
		} else if (kind == ack) {
			// when ack, close windows before or equal it
			unsigned int ack = ntohl(temp->head.ack);
			while (ntohl(window[win_begin]->head.seq) <= ack) {
				// close a window
				free(window[win_begin]);
				win_begin = (win_begin + 1) % WINDOW_SIZE_BACK_N_FRAME;
				win_num--;
				// when waiting queue is not empty, then pop one and push into window
				if (wait_num > 0) {
					window[win_end] = waiting[wait_begin];

					// try to send it
					SendFRAMEPacket((unsigned char *)window[win_end],
					window[win_end]->size);

					wait_begin = (wait_begin + 1) % max_wait_num;
					wait_num--;
					win_end = (win_end + 1) % WINDOW_SIZE_BACK_N_FRAME;
					win_num++;
				}
			}
		}
	}
	// when time out
	else if (messageType == MSG_TYPE_TIMEOUT) {
		UINT32 temp = ntohl(*((UINT32 *)pBuffer));
		int cursor = win_begin;

		// resend all frames after MSG_TYPE_TIMEOUT
		SendFRAMEPacket((unsigned char *)window[cursor], window[cursor]->size);
		cursor = (cursor + 1) % WINDOW_SIZE_BACK_N_FRAME;
		while (cursor != win_end) {
			SendFRAMEPacket((unsigned char *)window[cursor],
			window[cursor]->size);
			cursor = (cursor + 1) % WINDOW_SIZE_BACK_N_FRAME;
		}
	}
	return 0;
}
