/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#ifndef __cc_feedback_adapter_h_
#define __cc_feedback_adapter_h_

#include "sender_history.h"

#define FEEDBACK_RTT_WIN_SIZE 32
typedef struct
{
	sender_history_t*	hist;
	int32_t				min_feedback_rtt;
	int					index;
	int32_t				rtts[FEEDBACK_RTT_WIN_SIZE];

	int					num;
	packet_feedback_t	packets[MAX_FEELBACK_COUNT];
}feedback_adapter_t;

void				cc_feedback_adapter_init(feedback_adapter_t* adapter);
void				cc_feedback_adapter_destroy(feedback_adapter_t* adapter);

/*���һ�����緢�ͱ��ĵļ�¼*/
void				cc_feedback_add_packet(feedback_adapter_t* adapter, uint16_t seq, size_t size);

/*������������feedback����������packet_feedback�ṹ���У����������remote estimator proxy����������*/
int					cc_feedback_on_feedback(feedback_adapter_t* adapter, feedback_msg_t* msg);


#endif



