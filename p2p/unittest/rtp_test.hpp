#include "catch2/catch.hpp"
#include "../RtpSink.h"
#include "../RtpSource.h"
#include <random>
#include <chrono>
#include <thread>

TEST_CASE("Test rtp transport")
{
	asio::io_context io_context1;
	asio::io_context io_context2;

	std::shared_ptr<RtpSource> source(new RtpSource(io_context1));
	std::shared_ptr<RtpSink> sink(new RtpSink(io_context2));
	
	bool ret = false;

	ret = sink->Open();
	REQUIRE(ret == true);
	if (!ret) {
		return;
	}

	ret = source->Open(17675, 17676);
	REQUIRE(ret == true);
	if (!ret) {
		return;
	}
	
	std::random_device rd;
	uint32_t in_size = 102400;
	uint32_t out_size = in_size;
	std::shared_ptr<uint8_t> in_data(new uint8_t[in_size]);
	std::shared_ptr<uint8_t> out_data(new uint8_t[out_size]);

	for (uint32_t i = 0; i < in_size; i += 10) {
		in_data.get()[i] = rd() % 256;
	}

	int test_frames = 10;

	std::thread t1([&] {
		int num_frames = 0;
		source->SetMediaCB([out_data, &num_frames, &test_frames](std::shared_ptr<uint8_t> data, 
			size_t size, uint8_t type, uint32_t timestamp) {
			memcpy(out_data.get(), data.get(), size);
			num_frames++;
			if (num_frames == test_frames) {
				return false;
			}
			return true;
		});

		io_context1.run();
	});

	std::thread t2([&] {
		sink->SetPeerRtpAddress("127.0.0.1", 17675);
		int num_frames = test_frames;
		while (num_frames > 0) {
			num_frames--;
			sink->SendVideo(in_data, in_size);		
			io_context2.run();
			io_context2.restart();
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}		
	});

	t1.join();
	t2.join();

	bool is_error = false;
	for (uint32_t i = 0; i < out_size; i++) {
		if (out_data.get()[i] != in_data.get()[i]) {
			is_error = true;
			break;
		}
	}

	REQUIRE(is_error == false);
}
