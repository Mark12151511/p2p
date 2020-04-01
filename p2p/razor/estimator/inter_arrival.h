/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#ifndef __inter_arrival_h_
#define __inter_arrival_h_

#include <stdint.h>

/*һ�����籨�ĵ��շ�ʱ���, ������ͳ���㷨����ֲ��webRTC�е�GCC,�����ʱ��������ú��������㣬û�в���webRTC�е�AST��RTPʱ���������*/
typedef struct
{
	size_t				size;
	uint32_t			first_timestamp;
	uint32_t			timestamp;
	int64_t				complete_ts;
	int64_t				last_sys_ts;
}timestamp_group_t;

typedef struct
{
	timestamp_group_t	cur_ts_group;
	timestamp_group_t	prev_ts_group;

	int					num_consecutive;
	int					burst;

	uint32_t			time_group_len_ticks; /*һ��group�����ʱ�䷶Χ�� Ĭ����5����*/
}inter_arrival_t;


inter_arrival_t*		create_inter_arrival(int burst, uint32_t group_ticks);
void					destroy_inter_arrival(inter_arrival_t* arrival);

int						inter_arrival_compute_deltas(inter_arrival_t* arr, uint32_t timestamp, int64_t arrival_ts, int64_t system_ts, size_t size, 
													uint32_t* timestamp_delta, int64_t* arrival_delta, int* size_delta);


#endif
