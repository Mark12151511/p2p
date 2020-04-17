#include "catch2/catch.hpp"
#include "fec/fec.h"
#include <random>
#include <memory>

TEST_CASE("Test fec codec")
{
	std::random_device rd;
	fec::FecEncoder encoder;
	fec::FecDecoder decoder;
	fec::FecPackets out_packets;

	uint32_t in_size = 1024000;
	uint32_t out_size = in_size;
	uint8_t* in_data = new uint8_t[in_size];
	uint8_t* out_data = new uint8_t[out_size];
	memset(out_data, 0, out_size);

	// ��������
	for (uint32_t i = 0; i < in_size; i += 10) {
		in_data[i] = rd() % 256;
	}

	// FEC����
	// ������ 5% ,  FEC���� 15%
	// ������ 10% , FEC���� 20% 
	// ������ 15% , FEC���� 25% 
	// ������ 20% , FEC���� 30% 
	encoder.set_fec_percentage(10);
	int ret = encoder.encode(in_data, in_size, out_packets);
	REQUIRE(ret == 1);

	// �������(5%)
	uint32_t  dropped_packets = (uint32_t)(out_packets.size() * 0.05);
	for (uint32_t i = 0; i < dropped_packets; i++) {
		int index = (int)(rd() % out_packets.size());
		out_packets.erase(index);
	}

	// FEC����
	bool is_error = true;
	if (decoder.decode(out_packets, out_data, out_size) > 0) {
		is_error = false;
		// ��ԭ���ݰ��Ա�
		for (uint32_t i = 0; i < out_size; i++) {
			if (out_data[i] != in_data[i]) {
				is_error = true;
				break;
			}
		}
	}

	REQUIRE(is_error == true);

	delete[] in_data;
	delete[] out_data;
}