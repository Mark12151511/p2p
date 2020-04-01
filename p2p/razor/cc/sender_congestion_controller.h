/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#ifndef __sender_congestion_controller_h_
#define __sender_congestion_controller_h_

#include "cc_feedback_adapter.h"
#include "delay_base_bwe.h"
#include "bitrate_controller.h"
#include "pace_sender.h"
#include "ack_bitrate_estimator.h"

#include "razor_api.h"

typedef struct
{
	razor_sender_t				sender;
	int							accepted_queue_ms;			/*��Ƶ�����ڷ���queue������ӳ�*/
	int							was_in_alr;
	int32_t						rtt;

	delay_base_bwe_t*			bwe;						/*�����ӳٵĴ���������*/
	bitrate_controller_t*		bitrate_controller;			/*���ʿ������������bwe��ack rate��loss�����ۺ����ʵ���*/
	ack_bitrate_estimator_t*	ack;						/*Զ��ȷ���յ������ݴ���������*/
	pace_sender_t*				pacer;						/*���Ͷ˵Ĳ���������*/
	feedback_adapter_t			adapter;					/*��������Ϣ��������*/

	bin_stream_t				strm;

	void*						trigger;					/*���ʸı����Ҫ֪ͨ��ͨ�Ų��trigger*/
	bitrate_changed_func		trigger_cb;					/*֪ͨ����*/

}sender_cc_t;

sender_cc_t*					sender_cc_create(void* trigger, bitrate_changed_func bitrate_cb, void* handler, pace_send_func send_cb, int queue_ms);
void							sender_cc_destroy(sender_cc_t* cc);

void							sender_cc_heartbeat(sender_cc_t* cc);

/*packet_id�Ǳ�����ţ��൱��RTP��ͷ�е�SEQ*/
int								sender_cc_add_pace_packet(sender_cc_t* cc, uint32_t packet_id, int retrans, size_t size);
/*�����seq��transport��������ID����ʹ���ط������IDҲ�ǲ�һ����*/
void							sender_on_send_packet(sender_cc_t* cc, uint16_t seq, size_t size);

void							sender_on_feedback(sender_cc_t* cc, uint8_t* feedback, int feedback_size);

void							sender_cc_update_rtt(sender_cc_t* cc, int32_t rtt);
void							sender_cc_set_bitrates(sender_cc_t* cc, uint32_t min_bitrate, uint32_t start_bitrate, uint32_t max_bitrate);

int64_t							sender_cc_get_pacer_queue_ms(sender_cc_t* cc);
int64_t							sender_cc_get_first_packet_ts(sender_cc_t* cc);


#endif 



