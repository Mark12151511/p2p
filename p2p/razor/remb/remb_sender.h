/*-
* Copyright (c) 2017-2019 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/
#ifndef __remb_sender_h_
#define __remb_sender_h_

#include "rate_stat.h"
#include "cf_stream.h"
#include "bbr_pacer.h"
#include "razor_api.h"

typedef struct
{
	razor_sender_t				sender;
	
	/*��һ������������״̬*/
	uint32_t					last_bitrate_bps;
	uint8_t						last_fraction_loss;
	uint32_t					last_rtt;

	uint8_t						loss_fraction;

	void*						trigger;					/*���ʸı����Ҫ֪ͨ��ͨ�Ų��trigger*/
	bitrate_changed_func		trigger_cb;					/*֪ͨ����*/

	uint32_t					max_bitrate;
	uint32_t					min_bitrate;

	uint32_t					target_bitrate;

	bbr_pacer_t*				pacer;
	bin_stream_t				strm;
	rate_stat_t					rate;
}remb_sender_t;

remb_sender_t*					remb_sender_create(void* trigger, bitrate_changed_func bitrate_cb, void* handler, pace_send_func send_cb, int queue_ms, int padding);
void							remb_sender_destroy(remb_sender_t* sender);

void							remb_sender_heartbeat(remb_sender_t* s, int64_t now_ts);

int								remb_sender_add_pace_packet(remb_sender_t* s, uint32_t packet_id, int retrans, size_t size);
void							remb_sender_send_packet(remb_sender_t* s, uint16_t seq, size_t size);

void							remb_sender_on_feedback(remb_sender_t* s, uint8_t* feedback, int feedback_size);

void							remb_sender_update_rtt(remb_sender_t* s, int32_t rtt);
void							remb_sender_set_bitrates(remb_sender_t* s, uint32_t min_bitrate, uint32_t start_bitrate, uint32_t max_bitrate);

int64_t							remb_sender_get_pacer_queue_ms(remb_sender_t* s);
int64_t							remb_sender_get_first_packet_ts(remb_sender_t* s);

#endif

