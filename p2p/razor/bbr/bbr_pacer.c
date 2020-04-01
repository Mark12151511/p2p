/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/
#include "bbr_pacer.h"
#include "razor_log.h"

#define k_min_packet_limit_ms		5			/*������С���*/
#define k_max_packet_limit_ms		200			/*ÿ400������뷢��һ�����ģ���ֹ����ʧ��*/
#define k_max_interval_ms			30			/*�������ʱ����ʱ�䲻���ͱ���һ�η��ͺܶ����ݳ�ȥ�������籩*/
#define k_default_pace_factor		1.5			/*Ĭ�ϵ�pace factor����*/	


bbr_pacer_t* bbr_pacer_create(void* handler, pace_send_func send_cb, void* notify_handler, pacer_send_notify_cb notify_cb, uint32_t que_ms, int padding)
{
	bbr_pacer_t* pace = calloc(1, sizeof(bbr_pacer_t));
	pace->last_update_ts = GET_SYS_MS();
	pace->handler = handler;
	pace->send_cb = send_cb;
	pace->notify_handler = notify_handler;
	pace->notify_cb = notify_cb;
	pace->padding = padding;
	pace->factor = k_default_pace_factor;
	pace->congestion_window_size = 0xffffffff;

	pacer_queue_init(&pace->que, que_ms);

	init_interval_budget(&pace->media_budget, 0, -1);
	increase_budget(&pace->media_budget, k_min_packet_limit_ms);

	init_interval_budget(&pace->padding_budget, 0, -1);
	increase_budget(&pace->padding_budget, k_min_packet_limit_ms);

	return pace;
}

void bbr_pacer_destroy(bbr_pacer_t* pace)
{
	if (pace == NULL)
		return;

	pacer_queue_destroy(&pace->que);

	free(pace);
}

void bbr_pacer_set_estimate_bitrate(bbr_pacer_t* pace, uint32_t bitrate_bps)
{
	pace->estimated_bitrate = bitrate_bps;
	pace->pacing_bitrate_kpbs = SU_MAX(bitrate_bps / 1000, pace->min_sender_bitrate_kpbs) * pace->factor;
	razor_debug("set pacer bitrate, bitrate = %uKB/s\n", pace->pacing_bitrate_kpbs / 8);
}

void bbr_pacer_set_bitrate_limits(bbr_pacer_t* pace, uint32_t min_bitrate)
{
	pace->min_sender_bitrate_kpbs = min_bitrate / 1000;
	pace->pacing_bitrate_kpbs = SU_MAX(pace->estimated_bitrate / 1000, pace->min_sender_bitrate_kpbs) * pace->factor;
	/*razor_info("set pacer min bitrate, bitrate = %ubps\n", min_bitrate);*/
}

void bbr_pacer_set_pacing_rate(bbr_pacer_t* pace, uint32_t pacing_bitrate_kbps)
{
	pace->pacing_bitrate_kpbs = pacing_bitrate_kbps* pace->factor;
	//set_target_rate_kbps(&pace->padding_budget, pace->pacing_bitrate_kpbs);
} 

void bbr_pacer_set_padding_rate(bbr_pacer_t* pace, uint32_t padding_bitrate)
{
	set_target_rate_kbps(&pace->padding_budget, padding_bitrate);
}

void bbr_pacer_update_outstanding(bbr_pacer_t* pace, size_t outstanding_bytes)
{
	pace->outstanding_bytes = outstanding_bytes;
}

void bbr_pacer_update_congestion_window(bbr_pacer_t* pace, size_t congestion_window_size)
{
	pace->congestion_window_size = congestion_window_size;
}

void bbr_pacer_set_factor(bbr_pacer_t* pace, float factor)
{
	pace->factor = factor;
	bbr_pacer_set_estimate_bitrate(pace, pace->estimated_bitrate);
}

int bbr_pacer_insert_packet(bbr_pacer_t* pace, uint32_t seq, int retrans, size_t size, int64_t now_ts)
{
	packet_event_t ev;
	ev.seq = seq;
	ev.retrans = retrans;
	ev.size = size;
	ev.que_ts = now_ts;
	ev.sent = 0;
	return pacer_queue_push(&pace->que, &ev);
}

int64_t bbr_pacer_queue_ms(bbr_pacer_t* pace)
{
	return GET_SYS_MS() - pacer_queue_oldest(&pace->que);
}

size_t bbr_pacer_queue_size(bbr_pacer_t* pace)
{
	return pacer_queue_bytes(&pace->que);
}

static int bbr_pacer_congestion(bbr_pacer_t* pace)
{
	return 0;
	/*return pace->congestion_window_size <= pace->outstanding_bytes ? -1 : 0;*/
}

static int bbr_pacer_send(bbr_pacer_t* pace, packet_event_t* ev)
{
	/*���з��Ϳ���*/
	if (budget_remaining(&pace->media_budget) == 0 || bbr_pacer_congestion(pace) == -1)
		return -1;

	/*�����ⲿ�ӿڽ������ݷ���*/
	if (pace->send_cb != NULL)
		pace->send_cb(pace->handler, ev->seq, ev->retrans, ev->size, 0);

	if (pace->notify_cb != NULL)
		pace->notify_cb(pace->notify_handler, ev->size);

	use_budget(&pace->media_budget, ev->size);
	use_budget(&pace->padding_budget, ev->size);
	pace->outstanding_bytes += ev->size;

	return 0;
}

#define PADDING_SIZE 500
void bbr_pacer_try_transmit(bbr_pacer_t* pace, int64_t now_ts)
{
	int elapsed_ms;
	uint32_t target_bitrate_kbps;
	int sent_bytes;
	packet_event_t* ev;

	elapsed_ms = (int)(now_ts - pace->last_update_ts);

	if (elapsed_ms < k_min_packet_limit_ms)
		return;

	/*���ӵ���ˣ�ÿ400������뷢��һ��������ֹfeedback�޷�����*/
	if (elapsed_ms >= k_max_packet_limit_ms && bbr_pacer_congestion(pace) == -1){
		ev = pacer_queue_front(&pace->que);
		if (ev != NULL){
			if (pace->send_cb != NULL)
				pace->send_cb(pace->handler, ev->seq, ev->retrans, ev->size, 0);

			if (pace->notify_cb != NULL)
				pace->notify_cb(pace->notify_handler, ev->size);

			pace->outstanding_bytes += ev->size;
			pacer_queue_sent(&pace->que, ev);
		}

		pace->last_update_ts = now_ts;
	}
	else if (bbr_pacer_congestion(pace) == 0){
		elapsed_ms = SU_MIN(elapsed_ms, k_max_interval_ms);

		pace->last_update_ts = now_ts;
		/*����media budget����Ҫ������,�����µ�media budget֮��*/
		if (pacer_queue_bytes(&pace->que) > 0){
			target_bitrate_kbps = pacer_queue_target_bitrate_kbps(&pace->que, now_ts);
			target_bitrate_kbps = SU_MAX(pace->pacing_bitrate_kpbs, target_bitrate_kbps);
		}
		else
			target_bitrate_kbps = pace->pacing_bitrate_kpbs;
		/*���¼�����Է��͵�ֱ������*/
		set_target_rate_kbps(&pace->media_budget, target_bitrate_kbps);
		increase_budget(&pace->media_budget, elapsed_ms);
		increase_budget(&pace->padding_budget, elapsed_ms);

		/*���з���*/
		sent_bytes = 0;
		while (pacer_queue_empty(&pace->que) != 0){
			ev = pacer_queue_front(&pace->que);
			if (bbr_pacer_send(pace, ev) == 0)
				pacer_queue_sent(&pace->que, ev);
			else
				break;
		}

		/*����Ƿ����padding*/
		while (pace->padding == 1 && budget_remaining(&pace->padding_budget) > PADDING_SIZE / 4){
			if (bbr_pacer_congestion(pace) == -1)
				break;
			
			if (pace->send_cb != NULL){
				pace->send_cb(pace->handler, 0, 0, PADDING_SIZE, 1);

				if (pace->notify_cb != NULL)
					pace->notify_cb(pace->notify_handler, PADDING_SIZE);

				use_budget(&pace->media_budget, PADDING_SIZE);
				use_budget(&pace->padding_budget, PADDING_SIZE);
				pace->outstanding_bytes += PADDING_SIZE;
			}
			else 
				break;
		}
	}
}



