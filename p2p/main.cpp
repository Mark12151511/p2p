#include "fec/fec.h"
#include <random>

int main(int argc, char **argv)
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

	// 填充随机数
	for (uint32_t i = 0; i < in_size; i+=10) {
		in_data[i] = rd() % 256;
	}

	// FEC编码
	// 丢包率 5% ,  FEC比例 15%
	// 丢包率 10% , FEC比例 20% 
	// 丢包率 15% , FEC比例 25% 
	// 丢包率 20% , FEC比例 30% 
	encoder.set_fec_percentage(10); 
	if (encoder.encode(in_data, in_size, out_packets) == 0) {
		printf("fec encode succeed! \n");
	}
	else {
		printf("fec encode failed! \n");
	}
	
    // 随机丢包(5%)
	uint32_t  dropped_packets = (uint32_t)(out_packets.size() * 0.05);
	for (uint32_t i = 0; i < dropped_packets; i++) {
		int index = (int)(rd() % out_packets.size());
		out_packets.erase(index);
		//printf("drop packet, index: %d \n", index);
	}

	// FEC解码
	bool has_error = true;
	if (decoder.decode(out_packets, out_data, out_size) > 0) {
		has_error = false;
		// 与原数据包对比
		for (uint32_t i = 0; i < out_size; i++) {
			if (out_data[i] != in_data[i]) {			
				has_error = true;
				break;
			}
		}
	}

	if (!has_error) {
		printf("fec decode succeed! \n");
	}
	else {
		printf("fec decode failed! \n");
	}

	delete[] in_data;
	delete[] out_data;
	getchar();
	return 0;
}