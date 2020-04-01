/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/
#ifndef __bbr_pacer_h_
#define __bbr_pacer_h_

#include "pacer_queue.h"
#include "razor_api.h"
#include "interval_budget.h"

typedef void(*pacer_send_notify_cb)(void* handler, int size);

/*����û������gcc�е�pacer��ԭ������ΪBBR�ǻ��ڷ��ʹ��ں�pacing rate�����Ʒ���Ƶ�ʵģ�����GCCֻ�ǵ���ʹ��estimator rate�����Ʒ���Ƶ��*/

typedef struct
{
	uint32_t			min_sender_bitrate_kpbs;
	uint32_t			estimated_bitrate;
	uint32_t			pacing_bitrate_kpbs;

	int64_t				last_update_ts;
	int					padding;

	pacer_queue_t		que;						/*�ŶӶ���*/

	interval_budget_t	media_budget;				/*��ʽ��ý�屨�ĵķ����ٶȿ�����*/
	interval_budget_t	padding_budget;				/*���ķ����ٶȿ�����*/
	size_t				congestion_window_size;		/*ӵ�����ڴ�С*/
	size_t				outstanding_bytes;			/*����·�Ϸ��͵�����*/
	float				factor;						/*pacing rate�Ŵ�����*/
	/*�����ص�����*/
	void*				handler;
	pace_send_func		send_cb;

	void*				notify_handler;
	pacer_send_notify_cb notify_cb;
}bbr_pacer_t;

bbr_pacer_t*				bbr_pacer_create(void* handler, pace_send_func send_cb, void* notify_handler, pacer_send_notify_cb notify_cb, uint32_t que_ms, int padding);
void						bbr_pacer_destroy(bbr_pacer_t* pace);

void						bbr_pacer_set_estimate_bitrate(bbr_pacer_t* pace, uint32_t bitrate_pbs);
void						bbr_pacer_set_bitrate_limits(bbr_pacer_t* pace, uint32_t min_bitrate);
void						bbr_pacer_set_pacing_rate(bbr_pacer_t* pace, uint32_t pacing_bitrate);
void						bbr_pacer_set_padding_rate(bbr_pacer_t* pace, uint32_t padding_bitrate);
void						bbr_pacer_update_outstanding(bbr_pacer_t* pace, size_t outstanding_bytes);
void						bbr_pacer_update_congestion_window(bbr_pacer_t* pace, size_t congestion_window_size);
void						bbr_pacer_set_factor(bbr_pacer_t* pace, float factor);

int							bbr_pacer_insert_packet(bbr_pacer_t* pace, uint32_t seq, int retrans, size_t size, int64_t now_ts);

int64_t						bbr_pacer_queue_ms(bbr_pacer_t* pace);
size_t						bbr_pacer_queue_size(bbr_pacer_t* pace);

/*���Է���*/
void						bbr_pacer_try_transmit(bbr_pacer_t* pace, int64_t now_ts);
/*Ԥ�Ʒ��͵�queue�����ݵ�ʱ��*/
int64_t						bbr_pacer_expected_queue_ms(bbr_pacer_t* pace);
#endif



