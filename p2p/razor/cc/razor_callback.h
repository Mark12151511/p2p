/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#ifndef __razor_callback_function_h__
#define __razor_callback_function_h__

/*���ն˷�����Ϣ���ͺ������ǽ����ն˵���Ϣ���������Ͷ���*/
typedef void(*send_feedback_func)(void* handler, const uint8_t* payload, int payload_size);

/*���Ͷ˴���ı亯����trigger��ͨѶ�ϲ�����ʿ��Ʒ���������������ø���ģ������ʲ����ڶ�Ӧ��Ƶ����������*/
typedef void(*bitrate_changed_func)(void* trigger, uint32_t bitrate, uint8_t fraction_loss, uint32_t rtt);

/*���Ͷ�pacer�������ķ��͵Ļص�������
handler �Ƿ��Ͷ���
packet_id�Ǳ��ĵ�id��
retrans���ط���־
size�Ǳ��ĵĳ���
�������������ͨ��seq�ڷ��Ͷ������ҵ���Ӧ��packet��������packet����*/
typedef void(*pace_send_func)(void* handler, uint32_t packet_id, int retrans, size_t size, int padding);

/*��־����ص�����*/
typedef int(*razor_log_func)(int level, const char* file, int line, const char* fmt, va_list vl);

/*********************************���Ͷ�****************************************/
typedef struct __razor_sender razor_sender_t;

/*���Ͷ�ӵ�����ƶ�������������������5��10�������һ��*/
typedef void(*sender_heartbeat_func)(razor_sender_t* sender);

/*�������ʷ�Χ*/
typedef void(*sender_set_bitrates)(razor_sender_t* sender, uint32_t min_bitrate, uint32_t start_bitrate, uint32_t max_bitrate);

/*���Ͷ�����һ�������͵ı���*/
typedef int(*sender_add_packet_func)(razor_sender_t* sender, uint32_t packet_id, int retrans, size_t size);

/*�ϲ㽫һ�����ķ��͵��������Ҫ���䴫���������seq id�ͱ��Ĵ�С����ӵ�����ƶ����н��м�¼���Ա������ղ�ѯ*/
typedef void(*sender_on_send_func)(razor_sender_t* sender, uint16_t transport_seq, size_t size);

/*���ն˷�����������Ϣ�����뵽ӵ�����ƶ����н�����Ӧ����*/
typedef void(*sender_on_feedback_func)(razor_sender_t* sender, uint8_t* feedback, int feedback_size);

/*��������rtt*/
typedef void(*sender_update_rtt_func)(razor_sender_t* sender, int32_t rtt);

/*��ȡ����ӵ�������еȴ����Ͷ��е��ӳ�*/
typedef int(*sender_get_pacer_queue_ms_func)(razor_sender_t* sender);

/*��ȡ���͵�һ�����ĵ�ʱ���*/
typedef int64_t(*sender_get_first_ts)(razor_sender_t* sender);

struct __razor_sender
{
	int								type;
	int								padding;
	sender_heartbeat_func			heartbeat;
	sender_set_bitrates				set_bitrates;
	sender_add_packet_func			add_packet;
	sender_on_send_func				on_send;
	sender_on_feedback_func			on_feedback;
	sender_update_rtt_func			update_rtt;
	sender_get_pacer_queue_ms_func	get_pacer_queue_ms;
	sender_get_first_ts				get_first_timestamp;
};

/*************************************���ն�********************************************/
typedef struct __razor_receiver razor_receiver_t;

/*���ն�ӵ���������������*/
typedef void(*receiver_heartbeat_func)(razor_receiver_t* receiver);

/*���ն˽��յ�һ�����ģ�����ӵ������,
transport_seq		���͵ı��ĵ�����ͨ�����
timestamp			���Ͷ˵����ʱ�����ͨ����Ƶʱ����ͷ���ƫ��ʱ��������㣩��
size				�������ݴ�С������UDPͷ��Ӧ��Э���ͷ��С��
remb				�Ƿ����remb��ʽ�������ʣ�= 0��ʾ�ã�����ֵ��ʾ���ã����ֵ�����ڽ��յ��ı���ͷ
*/
typedef void(*receiver_on_received_func)(razor_receiver_t* receiver, uint16_t transport_seq, uint32_t timestamp, size_t size, int remb);

/*���������rtt*/
typedef void(*receiver_update_rtt_func)(razor_receiver_t* receiver, int32_t rtt);

/*������С����*/
typedef void(*receiver_set_min_bitrate_func)(razor_receiver_t*receiver, uint32_t bitrate);

/*�����������*/
typedef void(*receiver_set_max_bitrate_func)(razor_receiver_t*receiver, uint32_t bitrate);

struct __razor_receiver
{
	int								type;
	receiver_heartbeat_func			heartbeat;				/*���ն�ӵ����������������ÿ5����һ��*/
	receiver_on_received_func		on_received;			/*���ձ����¼�*/
	receiver_update_rtt_func		update_rtt;				/*����rtt*/
	receiver_set_max_bitrate_func	set_max_bitrate;		/*�������õ��������*/
	receiver_set_min_bitrate_func	set_min_bitrate;		/*�������õ���С����*/
};

#endif