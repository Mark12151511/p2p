/*
The MIT License

Copyright (c) 2018 polygraphene

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Github: https://github.com/polygraphene

*/

#include "fec.h"
#include "rs.h"
#include <assert.h>
#include <vector>
#include <chrono>
#include <mutex>
#include <algorithm>

namespace fec
{

static const int MAX_VIDEO_BUFFER_SIZE = MAX_PACKET_PAYLOAD_SIZE - sizeof(FecHeader);
static const int MAX_FEC_SHARDS = 250; // MAX_FEC_SHARDS >= data shards + parity_shards

/* 计算冗余分片数据 */
static int CalculateParityShards(int data_shards, int fec_percentage) 
{
	int total_parity_shards = (data_shards * fec_percentage + 99) / 100;
	return total_parity_shards;
}

/* 
单帧数据太大, 需要多个数据包共享一个分片。抗丢包能力会下降。
一个分片对应一个数据包, 效果最好, 但是对单帧数据大小有限制。

原因: 
假设有1000个数据包, 数据分片为50, 冗余分片为10, 总数据包个数为1200。
那么每20个数据包共享1个分片, 可以看成20组数据, 每组数据有60个数据包, 每组允许的最大丢包数则为10。
理论上丢包个数在200以内可以恢复, 前提是每个组均匀丢弃10个包。
如果一个组丢包数超过了10个包, 那个这个组是无法恢复的, 其他组可恢复, 但是单帧数据就不完整了。
*/
static int CalculateFECShardPackets(int len, int fec_percentage)
{
	int max_data_shards = ((MAX_FEC_SHARDS - 2) * 100 + 99 + fec_percentage) / (100 + fec_percentage);
	int min_block_size = (len + max_data_shards - 1) / max_data_shards;
	int shard_packets = (min_block_size + MAX_VIDEO_BUFFER_SIZE - 1) / MAX_VIDEO_BUFFER_SIZE;
	assert(max_data_shards + CalculateParityShards(max_data_shards, fec_percentage) <= MAX_FEC_SHARDS);
	return shard_packets;
}

FecCodec::FecCodec()
{
	static std::once_flag flag;
	std::call_once(flag, [] {
		reed_solomon_init();
	});	
}

FecCodec::~FecCodec()
{

}

int FecCodec::set_fec_percentage(int percentage)
{
	_fec_percentage = percentage;
	return 0;
}


FecEncoder::FecEncoder()
{
	
}

FecEncoder::~FecEncoder()
{

}

int FecEncoder::encode(uint8_t *in_data, uint32_t in_size, FecPackets& out_packets)
{
	out_packets.clear();

	int shard_packets = CalculateFECShardPackets(in_size, _fec_percentage);
	int block_size = shard_packets * MAX_VIDEO_BUFFER_SIZE;

	int data_shards = (in_size + block_size - 1) / block_size;
	int parity_shards = CalculateParityShards(data_shards, _fec_percentage);
	int total_shards = data_shards + parity_shards;

	assert(total_shards <= DATA_SHARDS_MAX);

	std::shared_ptr<reed_solomon> rs(reed_solomon_new(data_shards, parity_shards),
		[](reed_solomon *ptr) { reed_solomon_release(ptr); });

	std::vector<uint8_t *> shards(total_shards);

	for (int i = 0; i < data_shards; i++) {
		shards[i] = in_data + i * block_size;
	}

	if (in_size % block_size != 0) {
		shards[data_shards - 1] = new uint8_t[block_size];
		memset(shards[data_shards - 1], 0, block_size);
		memcpy(shards[data_shards - 1], in_data + (data_shards - 1) * block_size, in_size % block_size);
	}

	for (int i = 0; i < parity_shards; i++) {
		shards[data_shards + i] = new uint8_t[block_size];
	}

	int ret = reed_solomon_encode(rs.get(), &shards[0], total_shards, block_size);
	assert(ret == 0);

	FecHeader header;
	auto time_point = std::chrono::time_point_cast<std::chrono::microseconds>(std::chrono::steady_clock::now());

	memset(&header, 0, sizeof(header));
	header.fec_index = 0;
	header.fec_percentage = _fec_percentage;	
	header.total_size = in_size;
	header.fec_timestamp = (uint64_t)time_point.time_since_epoch().count();
	
	int data_remain = in_size;
	for (int i = 0; i < data_shards; i++) {		
		for (int j = 0; j < shard_packets; j++) {
			int copy_length = std::min(MAX_VIDEO_BUFFER_SIZE, data_remain);
			if (copy_length <= 0) {
				break;
			}

			header.fec_payload_size = copy_length;

			std::shared_ptr<FecPacket> packet_ptr = std::make_shared<FecPacket>();
			memcpy(&packet_ptr->header, &header, sizeof(FecHeader));
			memcpy(packet_ptr->payload, shards[i] + j * MAX_VIDEO_BUFFER_SIZE, copy_length);
			out_packets[header.fec_index] = packet_ptr;

			header.fec_index++;
			data_remain -= MAX_VIDEO_BUFFER_SIZE;
		}
	}

	header.fec_index = data_shards * shard_packets;

	for (int i = 0; i < parity_shards; i++) {
		for (int j = 0; j < shard_packets; j++) {
			int copy_length = MAX_VIDEO_BUFFER_SIZE;
			header.fec_payload_size = copy_length;

			std::shared_ptr<FecPacket> packet_ptr = std::make_shared<FecPacket>();
			memcpy(&packet_ptr->header, &header, sizeof(FecHeader));
			memcpy(packet_ptr->payload, shards[data_shards + i] + j * MAX_VIDEO_BUFFER_SIZE, copy_length);

			out_packets[header.fec_index] = packet_ptr;
			header.fec_index++;
		}
	}

	if (in_size % block_size != 0) {
		delete[] shards[data_shards - 1];
	}

	for (int i = 0; i < parity_shards; i++) {
		delete[] shards[data_shards + i];
	}

	return 0;
}

int FecDecoder::decode(FecPackets& in_packets, uint8_t *out_buf, uint32_t max_out_buf_size)
{
	auto& packet_header = in_packets.begin()->second->header;
	uint32_t total_size = packet_header.total_size;
	uint32_t fec_percentage = packet_header.fec_percentage;

	if (total_size > max_out_buf_size) {
		return -1;
	}

	uint32_t total_data_packets = (total_size + MAX_VIDEO_BUFFER_SIZE - 1) / MAX_VIDEO_BUFFER_SIZE;
	uint32_t shard_packets = CalculateFECShardPackets(total_size, fec_percentage);
	uint32_t block_size = shard_packets * MAX_VIDEO_BUFFER_SIZE;
	uint32_t data_shards = (total_size + block_size - 1) / block_size;
	uint32_t parity_shards = CalculateParityShards(data_shards, fec_percentage);
	uint32_t total_shards = data_shards + parity_shards;

	std::vector<std::vector<uint8_t>> marks(shard_packets);
	std::vector<uint8_t*> shards(total_shards);
	std::vector<bool> recovered_packet(shard_packets);
	std::vector<uint32_t> received_data_shards(shard_packets);
	std::vector<uint32_t> received_parity_shards(shard_packets);

	std::shared_ptr<reed_solomon> rs(reed_solomon_new(data_shards, parity_shards),
		[](reed_solomon *ptr) { reed_solomon_release(ptr); });

	for (uint32_t i = 0; i < shard_packets; i++) {
		marks[i].resize(total_shards);
		memset(&marks[i][0], 1, total_shards);
	}

	size_t padding = (shard_packets - total_data_packets % shard_packets) % shard_packets;
	for (size_t i = 0; i < padding; i++) {
		marks[shard_packets - i - 1][data_shards - 1] = 0;
		received_data_shards[shard_packets - i - 1]++;
	}

	std::vector<uint8_t> frame_buffer(total_shards * block_size);
	memset(&frame_buffer[0], 0, total_shards * block_size);

	for (auto iter : in_packets) {
		auto& packet = iter.second;
		uint8_t* buffer = &frame_buffer[packet->header.fec_index * MAX_VIDEO_BUFFER_SIZE];
		uint32_t size = packet->header.fec_payload_size;
		memcpy(buffer, packet->payload, size);
		if (size != MAX_VIDEO_BUFFER_SIZE) {
			memset(buffer + size, 0, MAX_VIDEO_BUFFER_SIZE - size);
		}

		uint32_t shard_index = packet->header.fec_index / shard_packets;
		uint32_t packet_index = packet->header.fec_index % shard_packets;
		marks[packet_index][shard_index] = 0;

		if (shard_index < data_shards) {
			received_data_shards[packet_index]++;
		}
		else {
			received_parity_shards[packet_index]++;
		}
	}

	for (uint32_t column = 0; column < shard_packets; column++) {
		if (recovered_packet[column]) {
			continue;
		}

		if (received_data_shards[column] == data_shards) {
			recovered_packet[column] = true;
			continue;
		}

		rs->shards = received_data_shards[column] + received_parity_shards[column];
		if (rs->shards < (int)data_shards) {
			return -1; // not enough parity data
		}

		for (uint32_t row = 0; row < total_shards; row++) {
			shards[row] = &frame_buffer[(row * shard_packets + column) * MAX_VIDEO_BUFFER_SIZE];
		}

		int result = reed_solomon_reconstruct(rs.get(), (uint8_t**)&shards[0], &marks[column][0],
			total_shards, MAX_VIDEO_BUFFER_SIZE);
		assert(result == 0);
		recovered_packet[column] = true;
	}

	memcpy(out_buf, &frame_buffer[0], total_size);
	return total_size;
}

}