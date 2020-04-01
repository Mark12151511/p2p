/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#include "bbr_controller.h"
#include "razor_log.h"

#ifndef false
#define false 0
#endif

#ifndef true
#define true 1
#endif

/////////////////////////////////////////////////////////////////////////////////////
static void bbr_enter_startup_mode(bbr_controller_t* bbr);
static void bbr_enter_probe_bandwidth_mode(bbr_controller_t* bbr, int64_t now_ts);
static int32_t bbr_get_congestion_window(bbr_controller_t* bbr);
static int64_t bbr_get_min_rtt(bbr_controller_t* bbr);
static double bbr_get_pacing_gain(bbr_controller_t* bbr, int index);
static int32_t bbr_pacing_rate(bbr_controller_t* bbr);
static int32_t bbr_bandwidth_estimate(bbr_controller_t* bbr);
static int bbr_is_probing_for_more_bandwidth(bbr_controller_t* bbr);

static int bbr_update_round_trip_counter(bbr_controller_t* bbr, int64_t last_acked_packet);
static int bbr_update_bandwidth_and_min_rtt(bbr_controller_t* bbr, int64_t now_ts, bbr_packet_info_t packets[], int size, uint32_t bitrate);
static void bbr_discard_lost_packets(bbr_controller_t* bbr, bbr_packet_info_t packets[], int size);

static void bbr_update_gain_cycle_phase(bbr_controller_t* bbr, int64_t now_ts, size_t prior_in_flight, int losses);
static void bbr_check_if_full_bandwidth_reached(bbr_controller_t* bbr);

static void bbr_maybe_exit_startup_or_drain(bbr_controller_t* bbr, bbr_feedback_t* feedback);
static void bbr_maybe_enter_or_exit_probe_rtt(bbr_controller_t* bbr, bbr_feedback_t* feedback, int is_round_start, int min_rtt_expired);

static void bbr_update_recovery_state(bbr_controller_t* bbr, int64_t last_acked_packet, int losses, int is_round_start);
static void bbr_update_ack_aggregation_bytes(bbr_controller_t* bbr, int64_t ack_time, size_t newly_acked_bytes);

static void bbr_calculate_pacing_rate(bbr_controller_t* bbr);
static void bbr_calculate_congestion_window(bbr_controller_t* bbr, size_t bytes_acked);
static void bbr_calculate_recovery_window(bbr_controller_t* bbr, size_t bytes_acked, size_t bytes_lost, size_t bytes_in_flight);
static void bbr_on_app_limited(bbr_controller_t* bbr, size_t bytes_in_flight);

/////////////////////////////////////////////////////////////////////////////////////

static inline void bbr_set_default_config(bbr_config_t* config)
{
	config->probe_bw_pacing_gain_offset = 0.25;
	config->encoder_rate_gain = 1;
	config->encoder_rate_gain_in_probe_rtt = 1;
	config->exit_startup_rtt_threshold_ms = 100000000; /*����һ���ܴ��ֵ��Ϊ��ʼֵ����˼�����ֵ�����ж�*/

	config->initial_congestion_window = kInitialCongestionWindowPackets * kDefaultTCPMSS;
	config->max_congestion_window = kDefaultMaxCongestionWindowPackets * kDefaultTCPMSS;
	config->min_congestion_window = kDefaultMinCongestionWindowPackets * kDefaultTCPMSS;

	config->probe_rtt_congestion_window_gain = 0.75;
	config->pacing_rate_as_target = false;

	config->exit_startup_on_loss = true;
	config->num_startup_rtts = 3;
	config->rate_based_recovery = false;
	config->max_aggregation_bytes_multiplier = 0;
	config->slower_startup = false;
	config->rate_based_startup = false;
	config->fully_drain_queue = false;
	config->initial_conservation_in_startup = CONSERVATION;
	config->max_ack_height_window_multiplier = 1;
	config->probe_rtt_based_on_bdp = true;
	config->probe_rtt_skipped_if_similar_rtt = false;
	config->probe_rtt_disabled_if_app_limited = false;
}

bbr_controller_t* bbr_create(bbr_target_rate_constraint_t* co, int32_t starting_bandwidth)
{
	bbr_controller_t* bbr = calloc(1, sizeof(bbr_controller_t));

	/*��ʼ��RTTͳ��ģ��*/
	bbr_rtt_init(&bbr->rtt_stat);

	/*��ʼ��BBRĬ�����ò���*/
	bbr_set_default_config(&bbr->config);

	bbr->sampler = sampler_create();

	/*����windows filter*/
	wnd_filter_init(&bbr->max_bandwidth, kBandwidthWindowSize, max_val_func);
	wnd_filter_init(&bbr->max_ack_height, kBandwidthWindowSize, max_val_func);
	bbr->default_bandwidth = kInitialBandwidthKbps;

	bbr->aggregation_epoch_start_time = -1;
	bbr->aggregation_epoch_bytes = 0;
	bbr->bytes_acked_since_queue_drained = 0;
	bbr->max_aggregation_bytes_multiplier = 0;

	bbr->min_rtt = 0;
	bbr->last_rtt = 0;

	bbr->min_rtt_timestamp = 0;

	/*��ʼ��ӵ������*/
	bbr->congestion_window = bbr->config.initial_congestion_window;
	bbr->initial_congestion_window = bbr->config.initial_congestion_window;
	bbr->max_congestion_window = bbr->config.max_congestion_window;
	bbr->min_congestion_window = bbr->config.min_congestion_window;

	bbr->pacing_gain = 1;
	bbr->pacing_rate = 0;

	bbr->congestion_window_gain_constant = kProbeBWCongestionWindowGain;
	bbr->rtt_variance_weight = kBbrRttVariationWeight;
	bbr->cycle_current_offset = 0;
	bbr->last_cycle_start = 0;
	bbr->is_at_full_bandwidth = false;
	bbr->rounds_without_bandwidth_gain = 0;
	bbr->bandwidth_at_last_round = 0;
	bbr->exiting_quiescence = false;
	bbr->exit_probe_rtt_at = -1;

	bbr->probe_rtt_round_passed = false;
	bbr->last_sample_is_app_limited = false;
	bbr->recovery_state = NOT_IN_RECOVERY;

	bbr->end_recovery_at = -1;
	bbr->recovery_window = bbr->max_congestion_window;
	bbr->app_limited_since_last_probe_rtt = false;
	bbr->min_rtt_since_last_probe_rtt = -1;

	razor_info("create bbr controller\n");

	bbr->constraints = *co;
	bbr->default_bandwidth = starting_bandwidth;

	bbr_reset(bbr);

	return bbr;
}

void bbr_destroy(bbr_controller_t* bbr)
{
	if (bbr == NULL)
		return;

	if (bbr->sampler != NULL){
		sampler_destroy(bbr->sampler);
		bbr->sampler = NULL;
	}

	free(bbr);
}

void bbr_reset(bbr_controller_t* bbr)
{
	bbr->round_trip_count = 0;
	bbr->rounds_without_bandwidth_gain = 0;
	if (bbr->config.num_startup_rtts > 0){
		bbr->is_at_full_bandwidth = false;
		bbr_enter_startup_mode(bbr);
	}
	else{
		bbr->is_at_full_bandwidth = true;
		bbr_enter_probe_bandwidth_mode(bbr, bbr->constraints.at_time);
	}
}

static bbr_network_ctrl_update_t bbr_create_rate_upate(bbr_controller_t* bbr, int64_t at_time)
{
	int32_t bandwidth, target_rate, pacing_rate;
	int64_t rtt;
	bbr_network_ctrl_update_t ret;

	ret.congestion_window = -1;
	if (at_time == -1)
		return ret;

	/*wnd_filter_print(&bbr->max_bandwidth);*/
	rtt = bbr_smoothed_rtt(&bbr->rtt_stat);

	/*����ӵ�����ƴ���*/
	ret.congestion_window = bbr_get_congestion_window(bbr);

	if (rtt <= 0)
		bandwidth = bbr->default_bandwidth;
	else
		bandwidth = ret.congestion_window / rtt;

	/*ȷ��pacing rate��target rate*/
	pacing_rate = bbr_pacing_rate(bbr);
	target_rate = bbr->config.pacing_rate_as_target ? pacing_rate : bandwidth;
	if (bbr->mode == PROBE_RTT)
		target_rate = (int32_t)(target_rate * bbr->config.encoder_rate_gain_in_probe_rtt);
	else
		target_rate = (int32_t)(target_rate * bbr->config.encoder_rate_gain);

	if (bbr->constraints.at_time > 0){
		if (bbr->constraints.max_rate > 0){
			target_rate = SU_MIN(target_rate, bbr->constraints.max_rate);
			pacing_rate = SU_MIN(pacing_rate, bbr->constraints.max_rate);
		}
		if (bbr->constraints.min_rate > 0){
			target_rate = SU_MAX(target_rate, bbr->constraints.min_rate);
			pacing_rate = SU_MAX(pacing_rate, bbr->constraints.min_rate);
		}
	}

	/*����target_rate��Ϣ*/
	ret.target_rate.at_time = at_time;
	ret.target_rate.bandwidth = bandwidth;
	ret.target_rate.rtt = SU_MAX(rtt, 8);
	ret.target_rate.loss_rate_ratio = bbr_loss_filter_get(&bbr->loss_rate);
	ret.target_rate.bwe_period = rtt * kGainCycleLength;
	ret.target_rate.target_rate = target_rate;

	/*����pacer��Ϣ*/
	ret.pacer_config.at_time = at_time;
	ret.pacer_config.time_window = rtt > 20 ? (rtt / 4) : 5;
	ret.pacer_config.data_window = (size_t)(ret.pacer_config.time_window * pacing_rate);
	if (bbr_is_probing_for_more_bandwidth(bbr) == 1)
		ret.pacer_config.pad_window = ret.pacer_config.time_window * target_rate;
	else
		ret.pacer_config.pad_window = 0;

	return ret;
}

bbr_network_ctrl_update_t bbr_on_network_availability(bbr_controller_t* bbr, bbr_network_availability_t* av)
{
	bbr_reset(bbr);
	bbr_rtt_connection_migration(&bbr->rtt_stat);

	return bbr_create_rate_upate(bbr, av->at_time);
}

bbr_network_ctrl_update_t bbr_on_newwork_router_change(bbr_controller_t* bbr)
{
	/*��ʱû��ʵ��*/
	return bbr_create_rate_upate(bbr, -1);
}

void bbr_on_remote_bitrate_report(bbr_controller_t* bbr, bbr_remote_bitrate_report_t* report)
{

}

void bbr_on_loss_report(bbr_controller_t* bbr, bbr_loss_report_t* report)
{

}

void bbr_on_rtt_update(bbr_controller_t* bbr, bbr_rtt_update_t* info)
{

}

bbr_network_ctrl_update_t bbr_on_heartbeat(bbr_controller_t* bbr, int64_t now_ts)
{
	return bbr_create_rate_upate(bbr, now_ts);
}

bbr_network_ctrl_update_t bbr_on_target_rate_constraints(bbr_controller_t* bbr, bbr_target_rate_constraint_t* msg)
{
	bbr->constraints = *msg;
	return bbr_create_rate_upate(bbr, msg->at_time);
}

bbr_network_ctrl_update_t bbr_on_send_packet(bbr_controller_t* bbr, bbr_packet_info_t* packet)
{
	bbr->last_sent_packet = packet->seq;
	if (packet->data_in_flight == 0 && sampler_is_app_limited(bbr->sampler) == 1)
		bbr->exiting_quiescence = true;

	if (bbr->aggregation_epoch_start_time == -1)
		bbr->aggregation_epoch_start_time = packet->send_time;

	/*��¼���͵İ�������Ϣ*/
	sampler_on_packet_sent(bbr->sampler, packet->send_time, packet->seq, packet->size, packet->data_in_flight);

	return bbr_create_rate_upate(bbr, packet->send_time);
}

static int64_t bbr_get_min_rtt(bbr_controller_t* bbr)
{
	if (bbr->min_rtt == 0)
		return bbr_initial_rtt_us(&bbr->rtt_stat) / 1000;
	else
		return bbr->min_rtt;
}

static int bbr_is_probing_for_more_bandwidth(bbr_controller_t* bbr)
{
	return ((bbr->mode == PROBE_BW && bbr->pacing_gain > 1) || bbr->mode == STARTUP) ? true : false;
}

static inline int bbr_in_slow_start(bbr_controller_t* bbr)
{
	return bbr->mode == STARTUP ? 1 : 0;
}

static inline int bbr_in_recovery(bbr_controller_t* bbr)
{
	return bbr->recovery_state != NOT_IN_RECOVERY ? true : false;
}

static inline int bbr_can_send(bbr_controller_t* bbr, size_t bytes_in_flight)
{
	return bytes_in_flight < bbr_get_congestion_window(bbr) ? true : false;
}

static int32_t bbr_pacing_rate(bbr_controller_t* bbr)
{
	if (bbr->pacing_rate == 0)
		return (int32_t)(kHighGain * bbr->initial_congestion_window / bbr_get_min_rtt(bbr));
	else
		return bbr->pacing_rate;
}

static int32_t bbr_bandwidth_estimate(bbr_controller_t* bbr)
{
	return (int32_t)wnd_filter_best(&bbr->max_bandwidth);
}

/*����BDP�ķ��ʹ���С*/
static size_t bbr_get_target_congestion_window(bbr_controller_t* bbr, double gain)
{
	size_t bdp, congestion_window;
	
	bdp = (int32_t)(bbr_get_min_rtt(bbr) * bbr_bandwidth_estimate(bbr));
	congestion_window = (size_t)(gain * bdp);
	
	if (congestion_window <= 0)
		congestion_window = (size_t)(gain * bbr->initial_congestion_window);

	if (congestion_window < bbr->min_congestion_window)
		;/* razor_debug("bdp = %d, max bandwidth = %u, min rtt = %u\n", congestion_window, bbr_bandwidth_estimate(bbr), bbr_get_min_rtt(bbr));*/

	return SU_MAX(congestion_window, bbr->min_congestion_window);
}

/*��ȡ��probe_rttģʽ�µ�ӵ�����ڴ�С*/
static size_t bbr_probe_rtt_congestion_window(bbr_controller_t* bbr)
{
	if (bbr->config.probe_rtt_based_on_bdp)
		return bbr_get_target_congestion_window(bbr, bbr->config.probe_rtt_congestion_window_gain);
	else
		return bbr->min_congestion_window;
}

/*���bbr��ǰ��ӵ�����ڴ�С*/
static int32_t bbr_get_congestion_window(bbr_controller_t* bbr)
{
	/*�����������Сrttģʽ�£���Ҫȡ��С�Ĳ�������*/
	if (bbr->mode == PROBE_RTT)
		return bbr_probe_rtt_congestion_window(bbr);

	/*�������recovery�׶�����recover window��Լ��������Լ�����ֵ*/
	if (bbr_in_recovery(bbr) && !bbr->config.rate_based_recovery
		&& !(bbr->mode == STARTUP && bbr->config.rate_based_startup))
		return SU_MIN(bbr->congestion_window, bbr->recovery_window);

	return bbr->congestion_window;
}

bbr_network_ctrl_update_t bbr_on_feedback(bbr_controller_t* bbr, bbr_feedback_t* feedback, uint32_t bandwidth)
{
	int64_t	feedback_recv_time, last_acked_packet;
	int loss_num = 0, acked_num = 0, i;
	bbr_packet_info_t *last_sent_packet, loss_packets[MAX_BBR_FEELBACK_COUNT], acked_packets[MAX_BBR_FEELBACK_COUNT];
	size_t total_data_acked_before, data_acked_size, data_lost_size;
	int is_round_start = false, min_rtt_expired = false;

	feedback_recv_time = feedback->feedback_time;
	/*û�з�����Ԫ,ֱ�ӷ���*/
	if (feedback->packets_num <= 0)
		return bbr_create_rate_upate(bbr, feedback->feedback_time);

	/*ͳ��RTT*/
	last_sent_packet = &feedback->packets[feedback->packets_num - 1];
	bbr_rtt_update(&bbr->rtt_stat, last_sent_packet->recv_time - last_sent_packet->send_time, 0, feedback_recv_time);

	total_data_acked_before = sampler_total_data_acked(bbr->sampler);

	/*�Զ����Ĵ���*/
	loss_num = bbr_feedback_get_loss(feedback, loss_packets, MAX_BBR_FEELBACK_COUNT);
	bbr_discard_lost_packets(bbr, loss_packets, loss_num);

	/*��acked�Ĵ���*/
	acked_num = bbr_feedback_get_received(feedback, acked_packets, MAX_BBR_FEELBACK_COUNT);
	/*�Զ������ʽ�������*/
	bbr_loss_filter_update(&bbr->loss_rate, feedback_recv_time, feedback->packets_num, loss_num);

	if (acked_num > 0){
		last_acked_packet = acked_packets[acked_num - 1].seq;
		is_round_start = bbr_update_round_trip_counter(bbr, last_acked_packet);
		min_rtt_expired = bbr_update_bandwidth_and_min_rtt(bbr, feedback_recv_time, acked_packets, acked_num, bandwidth);

		bbr_update_recovery_state(bbr, last_acked_packet, loss_num > 0 ? true : false, is_round_start);

		data_acked_size = sampler_total_data_acked(bbr->sampler) - total_data_acked_before;
		bbr_update_ack_aggregation_bytes(bbr, feedback_recv_time, data_acked_size);
		if (bbr->max_aggregation_bytes_multiplier > 0){
			if (feedback->data_in_flight <= 1.25 * bbr_get_target_congestion_window(bbr, bbr->pacing_rate))
				bbr->bytes_acked_since_queue_drained = 0;
			else
				bbr->bytes_acked_since_queue_drained += data_acked_size;
		}
	}

	/*Handle logic specific to PROBE_BW mode*/
	if (bbr->mode == PROBE_BW)
		bbr_update_gain_cycle_phase(bbr, feedback_recv_time, feedback->prior_in_flight, loss_num > 0 ? true : false);

	/* Handle logic specific to STARTUP and DRAIN modes */
	if (is_round_start && !bbr->is_at_full_bandwidth)
		bbr_check_if_full_bandwidth_reached(bbr);
	
	bbr_maybe_exit_startup_or_drain(bbr, feedback);

	/*Handle logic specific to PROBE_RTT,�ж��Ƿ�Ҫ����RTT���������RTT����*/
	bbr_maybe_enter_or_exit_probe_rtt(bbr, feedback, is_round_start, min_rtt_expired);

	/*����ӵ�����ڴ�С*/
	data_acked_size = sampler_total_data_acked(bbr->sampler) - total_data_acked_before;
	data_lost_size = 0;
	for (i = 0; i < loss_num; ++i)
		data_lost_size += loss_packets[i].size;

	bbr_calculate_pacing_rate(bbr);
	bbr_calculate_congestion_window(bbr, data_acked_size);
	bbr_calculate_recovery_window(bbr, data_acked_size, data_lost_size, feedback->data_in_flight);

	/*����ͳ����ǰ�ƶ�*/
	if (acked_num > 0)
		sampler_remove_old(bbr->sampler, last_acked_packet);

	return bbr_create_rate_upate(bbr, feedback->feedback_time);
}

/*bbr����STARTUPģʽ*/
static void bbr_enter_startup_mode(bbr_controller_t* bbr)
{
	bbr->mode = STARTUP;
	bbr->pacing_gain = kHighGain;
	bbr->congestion_window_gain = kHighGain;
}

static double bbr_get_pacing_gain(bbr_controller_t* bbr, int index)
{
	if (index == 0)
		return 1 + bbr->config.probe_bw_pacing_gain_offset;
	else if (index == 1)
		return 1 - bbr->config.probe_bw_pacing_gain_offset;
	else
		return 1;
}
/*bbr�����������ģʽ*/
static void bbr_enter_probe_bandwidth_mode(bbr_controller_t* bbr, int64_t now_ts)
{
	bbr->mode = PROBE_BW;
	bbr->congestion_window_gain = bbr->congestion_window_gain_constant;

	bbr->cycle_current_offset = rand() % (kGainCycleLength - 1);
	if (bbr->cycle_current_offset >= 1)
		bbr->cycle_current_offset += 1;

	bbr->last_cycle_start = now_ts;
	bbr->pacing_gain = bbr_get_pacing_gain(bbr, bbr->cycle_current_offset);
}

static void bbr_discard_lost_packets(bbr_controller_t* bbr, bbr_packet_info_t packets[], int size)
{
	int i;
	for (i = 0; i < size; ++i)
		sampler_on_packet_lost(bbr->sampler, packets[i].seq);
}

/*�ж��Ƿ����һ��round trip���ڣ�����ǣ����м�����+1����������һ�ڵ�round trip��λ����Ϣ*/
static int bbr_update_round_trip_counter(bbr_controller_t* bbr, int64_t last_acked_packet)
{
	if (last_acked_packet > bbr->current_round_trip_end){
		bbr->round_trip_count++;
		bbr->current_round_trip_end = bbr->last_sent_packet;
		return true;
	}
	else
		return false;
}

#define kMinRttExpiry 10000
static int bbr_should_extend_min_rtt_expiry(bbr_controller_t* bbr)
{
	if (bbr->config.probe_rtt_disabled_if_app_limited && bbr->app_limited_since_last_probe_rtt)
		return true;

	if (bbr->config.probe_rtt_skipped_if_similar_rtt && bbr->app_limited_since_last_probe_rtt
		&& bbr->min_rtt_since_last_probe_rtt <= bbr->min_rtt * kSimilarMinRttThreshold)
		return true;

	return false;
}

static int bbr_update_bandwidth_and_min_rtt(bbr_controller_t* bbr, int64_t now_ts, bbr_packet_info_t packets[], int size, uint32_t bandwidth)
{
	int i, min_rtt_expired;
	int64_t sample_rtt = -1;
	bbr_bandwidth_sample_t sample;

	for (i = 0; i < size; ++i){
		sample = sampler_on_packet_acked(bbr->sampler, packets[i].recv_time, packets[i].seq);
		bbr->last_sample_is_app_limited = sample.is_app_limited;

		/*��RTT�ļ���*/
		if (sample.rtt > 0){
			if (sample_rtt == -1)
				sample_rtt = sample.rtt;
			else
				sample_rtt = SU_MIN(sample_rtt, sample.rtt);
		}
	}

	if (sample_rtt == -1)
		return false;

	/*���д���ͳ�ƺ��˲�*/
	if (!sample.is_app_limited || sample.bandwidth > bbr_bandwidth_estimate(bbr)){
		wnd_filter_update(&bbr->max_bandwidth, sample.bandwidth, bbr->round_trip_count); /*bandwidth <= bbr->constraints.min_rate ? sample.bandwidth : bandwidth*/
	}

	bbr->last_rtt = sample_rtt;
	if (bbr->min_rtt_since_last_probe_rtt == -1)
		bbr->min_rtt_since_last_probe_rtt = sample_rtt;
	else
		bbr->min_rtt_since_last_probe_rtt = SU_MIN(bbr->min_rtt_since_last_probe_rtt, sample_rtt);

	/*�����Ƿ�ҪRTT������ʱ���Ƿ����*/
	min_rtt_expired = (bbr->min_rtt > 0 && now_ts > (bbr->min_rtt_timestamp + kMinRttExpiry)) ? true : false;
	if (min_rtt_expired || sample_rtt < bbr->min_rtt || bbr->min_rtt <= 0){
		if (bbr_should_extend_min_rtt_expiry(bbr))
			min_rtt_expired = false;
		else
			bbr->min_rtt = SU_MAX(5, sample_rtt);

		bbr->min_rtt_timestamp = now_ts;
		bbr->min_rtt_since_last_probe_rtt = -1;
		bbr->app_limited_since_last_probe_rtt = false;
	}

	return min_rtt_expired;
}

/*�������probe_bwģʽ�£����Խ���pacing_gain�����Ŵ��Դ���̽�����Ĵ���*/
static void bbr_update_gain_cycle_phase(bbr_controller_t* bbr, int64_t now_ts, size_t prior_in_flight, int losses)
{
	int gain_cycling;

	gain_cycling = (now_ts - bbr->last_cycle_start > bbr_get_min_rtt(bbr)) ? true : false;

	// If the pacing gain is above 1.0, the connection is trying to probe the
	// bandwidth by increasing the number of bytes in flight to at least
	// pacing_gain * BDP.  Make sure that it actually reaches the target, as long
	// as there are no losses suggesting that the buffers are not able to hold
	// that much.
	if (bbr->pacing_gain > 1.0 && !losses && prior_in_flight < bbr_get_target_congestion_window(bbr, bbr->pacing_gain))
		gain_cycling = false;

	// If pacing gain is below 1.0, the connection is trying to drain the extra
	// queue which could have been incurred by probing prior to it.  If the number
	// of bytes in flight falls down to the estimated BDP value earlier, conclude
	// that the queue has been successfully drained and exit this cycle early.	
	if (bbr->pacing_gain < 1.0 && prior_in_flight < bbr_get_target_congestion_window(bbr, 1))
		gain_cycling = true;

	if (gain_cycling){
		bbr->cycle_current_offset = (bbr->cycle_current_offset + 1) % kGainCycleLength;
		bbr->last_cycle_start = now_ts;

		if (bbr->config.fully_drain_queue && bbr->pacing_gain < 1 && bbr_get_pacing_gain(bbr, bbr->cycle_current_offset) == 1
			&& prior_in_flight < bbr_get_target_congestion_window(bbr, 1))
			return;

		bbr->pacing_gain = bbr_get_pacing_gain(bbr, bbr->cycle_current_offset);
	}
}

/*��STARTUP��DRAIN״̬�µĴ�������жϣ��жϵ�ǰ�����Ƿ�ﵽ��·��ߣ���ﵽ��ߣ�����Ϊ���������ʾ*/
static void bbr_check_if_full_bandwidth_reached(bbr_controller_t* bbr)
{
	int32_t target;

	/*�ϲ�Ӧ����ͣ��bbr����ģʽ*/
	if (bbr->last_sample_is_app_limited)
		return;

	target = (int32_t)(bbr->bandwidth_at_last_round * kStartupGrowthTarget);
	if (target <= bbr_bandwidth_estimate(bbr)){ /*����������������Ԥ�ڵ�������˵�����������ʻ��пռ䣬������������ֱ������full bandwidth*/
		bbr->bandwidth_at_last_round = bbr_bandwidth_estimate(bbr);
		bbr->rounds_without_bandwidth_gain = 0;
	}
	else{
		bbr->rounds_without_bandwidth_gain++;

		if (bbr->rounds_without_bandwidth_gain >= bbr->config.num_startup_rtts
			|| (bbr->config.exit_startup_on_loss && bbr_in_recovery(bbr))) /*��in recovery״̬�¿����ж�Ϊfull bandwidth*/
			bbr->is_at_full_bandwidth = true;
	}
}

static void bbr_maybe_exit_startup_or_drain(bbr_controller_t* bbr, bbr_feedback_t* feedback)
{
	int64_t exit_threshold_ms = bbr->config.exit_startup_rtt_threshold_ms;
	int rtt_over_threshold;

	rtt_over_threshold = (exit_threshold_ms > 0 && bbr->last_rtt - bbr->min_rtt > exit_threshold_ms) ? true : false;
	/*�����startupģʽ������״̬�Ѿ��ǵ��˴��������������ӳٳ�������ֵ���л���DRAINģʽ*/
	if (bbr->mode == STARTUP && (bbr->is_at_full_bandwidth || rtt_over_threshold)){
		bbr->mode = DRAIN;
		bbr->pacing_gain = kDrainGain;
		bbr->congestion_window_gain = kDrainGain;
	}

	/*�����drainģʽ���������ڴ��������С�ڵ�ǰ��ӵ�����ڣ�˵���д����࣬���Խ���probe_bwģʽ����������̽��*/
	if (bbr->mode == DRAIN && feedback->data_in_flight <= bbr_get_target_congestion_window(bbr, 1))
		bbr_enter_probe_bandwidth_mode(bbr, feedback->feedback_time);
}

/*�ж��Ƿ��������˳�PROBE_RTTģʽ*/
static void bbr_maybe_enter_or_exit_probe_rtt(bbr_controller_t* bbr, bbr_feedback_t* feedback, int is_round_start, int min_rtt_expired)
{
	/*���������min_rtt�����ҵ�ǰ����PROBE_RTTģʽ�£��л���PROBE_RTTģʽ��*/
	if (min_rtt_expired && !bbr->exiting_quiescence && bbr->mode != PROBE_RTT){
		bbr->mode = PROBE_RTT;
		bbr->pacing_gain = 1;
		bbr->exit_probe_rtt_at = -1;
	}

	if (bbr->mode == PROBE_RTT){
		/*��ͣ���ڷ��ͱ��ĵĴ����⣬��ֹ�����½�̫��������min_rtt��׼*/
		sampler_on_app_limited(bbr->sampler);

		if (bbr->exit_probe_rtt_at < 0){
			/*���ڷ��͵��������䵽probe_rttģʽ�µ�ӵ����С��������СRTT�ɼ�,����200����*/
			if (feedback->data_in_flight < bbr_probe_rtt_congestion_window(bbr) + kMaxPacketSize){
				bbr->exit_probe_rtt_at = feedback->feedback_time + kProbeRttTimeMs;
				bbr->probe_rtt_round_passed = false;
			}
		}
		else{
			/*����һ��RTT���ڣ������л�probe_rtt*/
			if (is_round_start)
				bbr->probe_rtt_round_passed = true;

			if (feedback->feedback_time >= bbr->exit_probe_rtt_at && bbr->probe_rtt_round_passed){
				/*probe_rtt��ϣ���¼�������Чʱ��*/
				bbr->min_rtt_timestamp = feedback->feedback_time;
				/*�л�BBR��ģʽ*/
				if (!bbr->is_at_full_bandwidth)
					bbr_enter_startup_mode(bbr);
				else
					bbr_enter_probe_bandwidth_mode(bbr, feedback->feedback_time);
			}
		}
	}

	bbr->exiting_quiescence = false;
}

static void bbr_update_recovery_state(bbr_controller_t* bbr, int64_t last_acked_packet, int losses, int is_round_start)
{
	/*�����������,recovery�ͽ�����ת����CONSERVATION״̬����¼���л�ʱ���һ����ACK�İ���ţ���֤���´�̽����������һ��FEEDBACKʱ������*/
	if (losses)
		bbr->end_recovery_at = last_acked_packet;

	switch (bbr->recovery_state){
	case NOT_IN_RECOVERY:
		/*�����������л���CONSERVATION,���п�ָ��ж�*/
		if (losses){
			bbr->recovery_state = CONSERVATION;
			if (bbr->mode == STARTUP)
				bbr->recovery_state = bbr->config.initial_conservation_in_startup;

			bbr->recovery_window = 0;
			/*��ʾ����һ��round trip���ڣ�������һ��round trip����*/
			bbr->current_round_trip_end = last_acked_packet;
		}
		break;

	case CONSERVATION:
	case MEDIUM_GROWTH:
		if (is_round_start)
			bbr->recovery_state = GROWTH;

	case GROWTH:
		/*����û�ж����������ڱ���GROWTH����һ��feedback���ڣ�ֹͣrecovery״̬*/
		if (!losses && (bbr->end_recovery_at == -1 || bbr->end_recovery_at < last_acked_packet))
			bbr->recovery_state = NOT_IN_RECOVERY;
		break;
	}
}

static void bbr_update_ack_aggregation_bytes(bbr_controller_t* bbr, int64_t ack_time, size_t newly_acked_bytes)
{
	size_t expected_bytes_acked;
	int32_t bandwidth;

	if (bbr->aggregation_epoch_start_time == -1)
		return;

	bandwidth = wnd_filter_best(&bbr->max_bandwidth);
	expected_bytes_acked = (size_t)(bandwidth * (ack_time - bbr->aggregation_epoch_start_time));

	if (bandwidth <= 0)
		return;

	if (bbr->aggregation_epoch_bytes <= expected_bytes_acked){
		bbr->aggregation_epoch_bytes = newly_acked_bytes;
		bbr->aggregation_epoch_start_time = ack_time;
		return;
	}

	bbr->aggregation_epoch_bytes += newly_acked_bytes;
	wnd_filter_update(&bbr->max_ack_height, bbr->aggregation_epoch_bytes - expected_bytes_acked, bbr->round_trip_count);
}

static void bbr_calculate_pacing_rate(bbr_controller_t* bbr)
{
	int32_t target_rate;
	if (bbr_bandwidth_estimate(bbr) <= 0)
		return;

	target_rate = (int32_t)(bbr->pacing_gain * bbr_bandwidth_estimate(bbr));

	if (bbr->config.rate_based_recovery && bbr_in_recovery(bbr))
		bbr->pacing_rate = (int32_t)(bbr->pacing_gain * wnd_filter_third_best(&bbr->max_bandwidth));

	if (bbr->is_at_full_bandwidth){
		bbr->pacing_rate = bbr_get_congestion_window(bbr) / bbr_smoothed_rtt(&bbr->rtt_stat);
		bbr->pacing_rate = SU_MAX(target_rate, bbr->pacing_rate);
		return;
	}

	/*��ʼ�׶Σ��ó�ʼ����ӵ�����ڼ�������õ�����*/
	if (bbr->pacing_rate == 0 && bbr_min_rtt(&bbr->rtt_stat) > 0){
		bbr->pacing_rate = (int32_t)(bbr->initial_congestion_window / (bbr_min_rtt(&bbr->rtt_stat)));
		return;
	}

	/*����������,��һ����������*/
	if (bbr->config.slower_startup && bbr->end_recovery_at > 0){
		bbr->pacing_rate = (int32_t)(kStartupAfterLossGain * bbr_bandwidth_estimate(bbr));
		return;
	}

	bbr->pacing_rate = SU_MAX(bbr->pacing_rate, target_rate);
}

static void bbr_calculate_congestion_window(bbr_controller_t* bbr, size_t bytes_acked)
{
	size_t target_window;

	/*PROBE_RTTģʽ�²��ı�ӵ������,һ���ǲ�����С����ģʽ����*/
	if (bbr->mode == PROBE_RTT)
		return;

	target_window = bbr_get_target_congestion_window(bbr, bbr->congestion_window_gain);

	if (bbr->rtt_variance_weight > 0 && bbr_bandwidth_estimate(bbr) > 0){
		target_window += (size_t)(bbr->rtt_variance_weight * bbr_mean_deviation(&bbr->rtt_stat) * bbr_bandwidth_estimate(bbr));
	}
	else if (bbr->max_aggregation_bytes_multiplier > 0 && bbr->is_at_full_bandwidth){
		if (bbr->max_aggregation_bytes_multiplier * wnd_filter_best(&bbr->max_ack_height) > bbr->bytes_acked_since_queue_drained / 2)
			target_window += (size_t)(bbr->max_aggregation_bytes_multiplier * wnd_filter_best(&bbr->max_ack_height) - bbr->bytes_acked_since_queue_drained / 2);
	}
	else if (bbr->is_at_full_bandwidth)
		target_window += (size_t)(wnd_filter_best(&bbr->max_ack_height));

	if (bbr->is_at_full_bandwidth)
		bbr->congestion_window = SU_MIN(target_window, bbr->congestion_window + bytes_acked);
	else if (bbr->congestion_window < target_window || sampler_total_data_acked(bbr->sampler) < bbr->initial_congestion_window)
		bbr->congestion_window = bbr->congestion_window + bytes_acked;

	bbr->congestion_window = SU_MAX(bbr->congestion_window, bbr->min_congestion_window);
	bbr->congestion_window = SU_MIN(bbr->congestion_window, bbr->max_congestion_window);
}

/*��������������Ҫ����recovery���㣬��ָ�����*/
static void bbr_calculate_recovery_window(bbr_controller_t* bbr, size_t bytes_acked, size_t bytes_lost, size_t bytes_in_flight)
{
	if (bbr->config.rate_based_recovery || bbr->config.rate_based_startup && bbr->mode == STARTUP)
		return;

	/*���ڻָ����ͽ׶Σ�������recovery window��С*/
	if (bbr->recovery_state == NOT_IN_RECOVERY)
		return;

	if (bbr->recovery_window == 0){
		bbr->recovery_window = bytes_in_flight + bytes_acked;
		bbr->recovery_window = SU_MAX(bbr->min_congestion_window, bbr->recovery_window);
		return;
	}

	bbr->recovery_window = ((bbr->recovery_window >= bytes_lost) ? (bbr->recovery_window - bytes_lost) : kMaxSegmentSize);

	/*����״̬�ӳ�*/
	if (bbr->recovery_state == GROWTH)
		bbr->recovery_window += bytes_acked;
	else if (bbr->recovery_window == MEDIUM_GROWTH)
		bbr->recovery_window += bytes_acked / 2;

	bbr->recovery_window = SU_MAX(bbr->recovery_window, bytes_in_flight + bytes_acked);
	bbr->recovery_window = SU_MAX(bbr->min_congestion_window, bbr->recovery_window);
}

static void bbr_on_app_limited(bbr_controller_t* bbr, size_t bytes_in_flight)
{
	if (bytes_in_flight >= bbr_get_congestion_window(bbr))
		return;

	bbr->app_limited_since_last_probe_rtt = true;
	sampler_on_app_limited(bbr->sampler);
}













