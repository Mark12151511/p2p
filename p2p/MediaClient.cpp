#include "MediaClient.h"
#include "message.hpp"
#include "log.h"

#define TEST_TOKEN "12345"

using namespace xop;

MediaClient::MediaClient()
{

}

MediaClient::~MediaClient()
{

}

bool MediaClient::Connect(const char* ip, uint16_t port, uint32_t timeout_msec)
{
	std::lock_guard<std::mutex> locker(mutex_);

	static std::once_flag flag;
	std::call_once(flag, []{
		enet_initialize();
	});

	if (!event_client_.Connect(ip, port, timeout_msec)) {
		LOG(" Event client connect failed.");
	}

	SendActive();

	is_started_ = true;
	event_thread_.reset(new std::thread([this] {
		EventLoop();
	}));

	return true;
}

void MediaClient::Close()
{
	std::lock_guard<std::mutex> locker(mutex_);

	if (is_started_) {
		is_started_ = false;
		event_thread_->join();
		event_thread_ = nullptr;
		event_client_.Close();
	}
}

void MediaClient::SendActive()
{
	if (event_client_.IsConnected()) {
		std::string token = TEST_TOKEN;
		ByteArray byte_array;

		ActiveMsg msg(token.c_str(), token.size() + 1);
		int size = msg.Encode(byte_array);
		if (size > 0) {
			event_client_.Send(byte_array.Data(), size);
		}
	}
}

void MediaClient::EventLoop()
{
	uint32_t msec = 5;
	uint32_t cid = 0;
	uint32_t max_message_len = 1500;
	std::shared_ptr<uint8_t> message(new uint8_t[max_message_len]);

	while (is_started_)
	{
		int msg_len = event_client_.Recv(message.get(), max_message_len, msec);
		if (msg_len > 0) {
			int msg_type = message.get()[0];
			ByteArray byte_array((char*)message.get(), msg_len);

			if (msg_type == MSG_ACTIVE_ACK) {
				ActiveAckMsg active_ack_msg;
				active_ack_msg.Decode(byte_array);
				printf("recv ack!\n");
			}
		}
	}
}