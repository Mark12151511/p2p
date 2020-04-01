/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#include "sender_bandwidth_estimator.h"
#include "razor_log.h"

#define k_low_loss_threshold			0.02f
#define k_high_loss_threshold			0.1f
#define k_bitrate_threshold_kbps		0

#define k_bwe_increase_interval_ms		1000
#define k_max_bitrate_bps				1000000000
#define k_limit_num_packets				20
#define k_start_phase_ms				2000
#define k_feelback_interval_ms			5000
#define k_feelback_timeout_intervals	3
#define k_timeout_interval_ms			1000
#define k_bwe_decrease_interval_ms		300


static void cap_bitrate_to_threshold(sender_estimation_t* est, int64_t cur_ts, uint32_t bitrate);

sender_estimation_t* sender_estimation_create(uint32_t min_bitrate, uint32_t max_bitrate)
{
	sender_estimation_t* estimator = calloc(1, sizeof(sender_estimation_t));
	estimator->min_conf_bitrate = min_bitrate;
	estimator->max_conf_bitrate = max_bitrate;
	estimator->has_decreased_since_last_fraction_loss = -1;
	estimator->last_feelback_ts = -1;
	estimator->last_packet_report_ts = -1;
	estimator->last_timeout_ts = -1;
	estimator->first_report_ts = -1;

	estimator->low_loss_threshold = k_low_loss_threshold;
	estimator->high_loss_threshold = k_high_loss_threshold;
	estimator->bitrate_threshold = 1000 * k_bitrate_threshold_kbps;

	estimator->begin_index = 0;
	estimator->end_index = 0;
	estimator->prev_fraction_loss = -1;

	estimator->state = kBwNormal;

	return estimator;
}

void sender_estimation_destroy(sender_estimation_t* estimator)
{
	if(estimator != NULL)
		free(estimator);
}

uint32_t sender_estimation_get_min_bitrate(sender_estimation_t* est)
{
	return est->max_conf_bitrate;
}

void sender_estimation_set_minmax_bitrate(sender_estimation_t* estimation, uint32_t min_bitrate, uint32_t max_bitrate)
{
	estimation->min_conf_bitrate = SU_MAX(estimation->min_conf_bitrate, min_bitrate);
	if (max_bitrate > 0)
		estimation->max_conf_bitrate = SU_MAX(estimation->min_conf_bitrate, max_bitrate);
	else
		estimation->max_conf_bitrate = k_max_bitrate_bps;
}

void sender_estimation_set_send_bitrate(sender_estimation_t* est, uint32_t send_bitrate)
{
	razor_debug("sender_estimation_set_send_bitrate, bitrate = %u\n", send_bitrate);
	cap_bitrate_to_threshold(est, GET_SYS_MS(), send_bitrate);

	/*�����ʷ��С��¼*/
	est->end_index = est->begin_index = 0;
}

void sender_estimation_set_bitrates(sender_estimation_t* est, uint32_t send_bitrate, uint32_t min_bitrate, uint32_t max_bitrate)
{
	razor_debug("sender_estimation_set_bitrates, bitrate = %u", send_bitrate);
	sender_estimation_set_minmax_bitrate(est, min_bitrate, max_bitrate);
	sender_estimation_set_send_bitrate(est, send_bitrate);
}

void sender_estimation_update_remb(sender_estimation_t* est, int64_t cur_ts, uint32_t bitrate)
{
	razor_debug("sender_estimation_update_remb, bitrate = %u", bitrate);
	est->bwe_incoming = bitrate;
	cap_bitrate_to_threshold(est, cur_ts, est->curr_bitrate);
}

void sender_estimation_update_delay_base(sender_estimation_t* est, int64_t cur_ts, uint32_t bitrate, int state)
{
	razor_debug("sender_estimation_update_delay_base, bitrate = %u\n", bitrate);
	est->delay_base_bitrate = bitrate;
	est->state = state;
	cap_bitrate_to_threshold(est, cur_ts, est->curr_bitrate);
}

static double slope_filter_update(slope_filter_t* slope, int delta)
{
	slope->slope = 255;

	slope->index++;
	slope->acc -= slope->frags[slope->index % LOSS_WND_SIZE];
	slope->frags[slope->index % LOSS_WND_SIZE] = delta;
	slope->acc += delta;

	if (slope->index > LOSS_WND_SIZE)
		slope->slope = slope->acc * 1.0f / LOSS_WND_SIZE;
	else if (slope->index > 10)
		slope->slope = slope->acc * 1.0f / slope->index;

	return slope->slope;
}

/*���½��ն˻㱨�Ķ����ӳ�����*/
void sender_estimation_update_block(sender_estimation_t* est, uint8_t fraction_loss, uint32_t rtt, int number_of_packets, int64_t cur_ts, uint32_t acked_bitrate)
{
	int lost_packets_Q8;

	est->last_feelback_ts = cur_ts;
	if (est->first_report_ts == -1)
		est->first_report_ts = cur_ts;

	if (rtt > 0)
		est->last_rtt = rtt;

	if (number_of_packets > 0){
		lost_packets_Q8 = fraction_loss * number_of_packets;
		est->lost_packets_since_last_loss_update_Q8 += lost_packets_Q8;
		est->expected_packets_since_last_loss_update += number_of_packets;

		if (est->expected_packets_since_last_loss_update < k_limit_num_packets)
			return;

		/*���㶪���ʣ�������ô���20�����ĵĲ�����Ϊ��������*/
		est->has_decreased_since_last_fraction_loss = -1;
		est->last_fraction_loss = est->lost_packets_since_last_loss_update_Q8 / est->expected_packets_since_last_loss_update;

		est->lost_packets_since_last_loss_update_Q8 = 0;
		est->expected_packets_since_last_loss_update = 0;
		est->last_packet_report_ts = cur_ts;

		/*���ж����ʲ��˲��������жϳ���������������������ӵ������*/
		if (est->prev_fraction_loss != -1)
			slope_filter_update(&est->slopes, est->last_fraction_loss - (int)est->prev_fraction_loss);
		est->prev_fraction_loss = est->last_fraction_loss;

		sender_estimation_update(est, cur_ts, acked_bitrate);
	}
}

int sender_estimation_is_start_phare(sender_estimation_t* est, int64_t cur_ts)
{
	if (est->first_report_ts == -1 || cur_ts < est->first_report_ts + k_start_phase_ms)
		return 0;
	return -1;
}

/*������С������ʷ��¼*/
static void sender_estimation_update_history(sender_estimation_t* estimator, int64_t cur_ts)
{
	uint32_t i, pos;

	/*ɾ������1�����ļ�¼*/
	for (i = estimator->begin_index; i < estimator->end_index; ++i){
		pos = i % MIN_HISTORY_ARR_SIZE;
		if (estimator->min_bitrates[pos].ts + k_bwe_increase_interval_ms < cur_ts){
			estimator->begin_index = i + 1;
			estimator->min_bitrates[pos].bitrate = 0;
			estimator->min_bitrates[pos].ts = 0;
		}
		else 
			break;
	}

	pos = (estimator->end_index - 1) % MIN_HISTORY_ARR_SIZE;
	while (estimator->begin_index < estimator->end_index && estimator->min_bitrates[pos].bitrate >= estimator->curr_bitrate){
		/*������һ����¼�����ʴ��ڵ�ǰ��������Ĵ����޳�*/
		estimator->min_bitrates[pos].bitrate = 0;
		estimator->min_bitrates[pos].ts = 0;

		estimator->end_index--;
		pos = (estimator->end_index - 1) % MIN_HISTORY_ARR_SIZE;
	}

	if (estimator->end_index == estimator->begin_index)
		estimator->end_index = estimator->begin_index = 0;

	/*����µļ�¼*/
	pos = estimator->end_index % MIN_HISTORY_ARR_SIZE;
	if (estimator->end_index == estimator->begin_index + MIN_HISTORY_ARR_SIZE)
		estimator->begin_index++;

	estimator->end_index++;
	estimator->min_bitrates[pos].bitrate = estimator->curr_bitrate;
	estimator->min_bitrates[pos].ts = cur_ts;
}

static void cap_bitrate_to_threshold(sender_estimation_t* est, int64_t cur_ts, uint32_t bitrate)
{
	/*�Դ������ȡֵ��ȥREMB��delay base֮����Сֵ*/
	if (est->bwe_incoming > 0 && bitrate > est->bwe_incoming)
		bitrate = est->bwe_incoming;

	if (est->delay_base_bitrate > 0 && bitrate > est->delay_base_bitrate)
		bitrate = est->delay_base_bitrate;

	if (bitrate > est->max_conf_bitrate)
		bitrate = est->max_conf_bitrate;

	if (bitrate < est->min_conf_bitrate)
		bitrate = est->min_conf_bitrate;

	est->curr_bitrate = bitrate;
}

void sender_estimation_update(sender_estimation_t* est, int64_t cur_ts, uint32_t acked_bitrate)
{
	uint32_t new_bitrate = est->curr_bitrate, pos;
	int64_t time_since_packet_report_ms, time_since_feedback_ms;
	double loss;

	/*����ǿ�ʼ�׶�������û�з�������������ȡREMB��delay base�д���Ϊ��ǰ�ľ��ߴ���*/
	if (est->last_fraction_loss == 0 && sender_estimation_is_start_phare(est, cur_ts) == 0){
		new_bitrate = SU_MAX(est->bwe_incoming, new_bitrate);
		new_bitrate = SU_MAX(est->delay_base_bitrate, new_bitrate);

		if (new_bitrate != est->curr_bitrate){
			est->end_index = est->begin_index = 0;
			est->min_bitrates[est->end_index].ts = cur_ts;
			est->min_bitrates[est->end_index].bitrate = est->curr_bitrate;
			est->end_index++;

			razor_debug("sender_estimation_update start_phare, bitrate = %u\n", new_bitrate);
			cap_bitrate_to_threshold(est, cur_ts, new_bitrate);
			return;
		}
	}

	sender_estimation_update_history(est, cur_ts);

	if (est->last_packet_report_ts == -1){
		razor_debug("sender_estimation_update last_packet_report_ts = -1, bitrate = %u\n", est->curr_bitrate);
		cap_bitrate_to_threshold(est, cur_ts, est->curr_bitrate);
		return;
	}

	/*���ж�����ص����ʿ���*/
	time_since_packet_report_ms = cur_ts - est->last_packet_report_ts;
	time_since_feedback_ms = cur_ts - est->last_feelback_ts;
	if (time_since_packet_report_ms * 1.2 < k_feelback_interval_ms){
		/*����������0 ~ 255����ʾ�����������ݰ����ǲ���uint8_t��ʾ�����������ڼ����ʱ����Ҫת���ɰٷ����������ж�*/
		loss = est->last_fraction_loss / 256.0;

		if (est->slopes.slope < 0.8f && loss > est->low_loss_threshold && est->state != kBwOverusing){ /*�����ӳٲ�û�����󣬶����ʴ��ڹ̶��ķ�Χ�ڣ�˵�����������������д�����ռ*/
			pos = est->begin_index % MIN_HISTORY_ARR_SIZE;
			new_bitrate = (uint32_t)(est->min_bitrates[pos].bitrate * 1.08 + 0.5 + 1000);
		}
		else {
			/*���緢�Ͷ����� < 2%ʱ��������������*/
			if (est->curr_bitrate < est->bitrate_threshold || loss < est->low_loss_threshold){
				pos = est->begin_index % MIN_HISTORY_ARR_SIZE;
				new_bitrate = (uint32_t)(est->min_bitrates[pos].bitrate * 1.08 + 0.5 + 1000); /*1000�Ƿ�ֹmin_bitrate̫С��ɵ�����Ч*/
				/*razor_debug("sender_estimation_update, loss < 2, new_bitrate = %u\n", new_bitrate);*/
			}
			else if (est->curr_bitrate > est->bitrate_threshold){ /*���ʹ��أ����ݶ����ʽ��������½�����*/
				/* 2% < loss < 10%*/
				if (loss < est->high_loss_threshold){
					/*ά�ֵ�ǰ��������*/
				}
				else{ /*loss > 10%,���������½������ݶ�����ռ���������½���*/
					if (est->has_decreased_since_last_fraction_loss == -1 && cur_ts >= est->last_decrease_ts + k_bwe_decrease_interval_ms + est->last_rtt){
						est->last_decrease_ts = cur_ts;
						est->has_decreased_since_last_fraction_loss = 0;

						new_bitrate = (uint32_t)(est->curr_bitrate * (512 - est->last_fraction_loss) / 512.0f);
						if (acked_bitrate > 0)
							new_bitrate = SU_MAX(acked_bitrate, new_bitrate);
						razor_debug("sender_estimation_update, loss >= 10, new_bitrate = %u\n", new_bitrate);
					}
				}
			}
		}
	}
	else if (time_since_feedback_ms > k_feelback_timeout_intervals * k_feelback_interval_ms &&
		(est->last_timeout_ts == -1 || cur_ts > est->last_timeout_ts + k_timeout_interval_ms)){ /*feelback��Ϣ��ʧ����ʱ�����д����½��������ض���ͳ��*/
		new_bitrate = new_bitrate * 4 / 5;
		
		est->lost_packets_since_last_loss_update_Q8 = 0;
		est->expected_packets_since_last_loss_update = 0;

		est->last_timeout_ts = cur_ts;
	}


	/*����ȷ����ǰ���ô���*/
	cap_bitrate_to_threshold(est, cur_ts, new_bitrate);
}



