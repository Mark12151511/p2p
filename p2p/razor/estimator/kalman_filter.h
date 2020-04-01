/*-
* Copyright (c) 2017-2018 wenba, Inc.
*	All rights reserved.
*
* See the file LICENSE for redistribution information.
*/

#ifndef __kalman_filter_h_
#define __kalman_filter_h_

#include "cf_platform.h"
#include "cf_list.h"

/*�����ӳټ���kalman filterʵ�֣���Ҫ��ͨ��kalman filter��ȷ��δ�������Ŷ��ӳ٣�ͨ������Ŷ��ӳٲ����ж����縺��״̬*/

#define HISTORY_FRAME_SIZE 60

typedef struct
{	
	uint16_t num_of_deltas;
	double slope;
	double offset;
	double prev_offset;
	double E[2][2];
	double process_noise[2];
	double avg_noise;
	double var_noise;

	uint32_t index;
	double history[HISTORY_FRAME_SIZE];
}kalman_filter_t;

kalman_filter_t*	kalman_filter_create();
void				kalman_filter_destroy(kalman_filter_t* kalman);

void				kalman_filter_update(kalman_filter_t* kalman, int64_t arrival_ts_delta, double ts_delta, int size_delta, int state, int64_t now_ms);

#endif



