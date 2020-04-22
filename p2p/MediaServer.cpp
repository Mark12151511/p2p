#include "MediaServer.h"
#include "message.hpp"
#include "log.h"

#define TEST_TOKEN "12345"

using namespace xop;

MediaServer::MediaServer()
{

}

MediaServer::~MediaServer()
{

}

bool MediaServer::Start(const char* ip, uint16_t port)
{
	std::lock_guard<std::mutex> locker(mutex_);

	static std::once_flag flag;
	std::call_once(flag, [] {
		enet_initialize();
	});

	if (is_started_) {
		return false;
	}

	if (!event_server_.Start(ip, port, kMaxConnectios)) {
		LOG(" Event server start failed.");
		return false;
	}

	is_started_ = true;
	event_thread_.reset(new std::thread([this] {
		EventLoop();
	}));
	
	return true;
}

void MediaServer::Stop()
{
	std::lock_guard<std::mutex> locker(mutex_);

	if (is_started_) {
		is_started_ = false;
		event_thread_->join();
		event_thread_ = nullptr;
		event_server_.Stop();
	}
}

void MediaServer::SendActiveAck(uint32_t cid, uint32_t uid, uint32_t cseq)
{
	
	ByteArray byte_array;
	ActiveAckMsg msg;
	int size = msg.Encode(byte_array);
	if (size > 0) {
		event_server_.Send(cid, byte_array.Data(), size);
	}
}

void MediaServer::EventLoop()
{
	uint32_t msec = 5;
	uint32_t cid = 0;
	uint32_t max_message_len = 1500;
	std::shared_ptr<uint8_t> message(new uint8_t[max_message_len]);

	while (is_started_)  
	{
		int msg_len = event_server_.Recv(&cid, message.get(), max_message_len, msec);
		if (msg_len > 0) {
			int msg_type = message.get()[0];
			ByteArray byte_array((char*)message.get(), msg_len);
			if (msg_type == MSG_ACTIVE) {
				ActiveMsg active_msg;
				active_msg.Decode(byte_array);
				if (strcmp(TEST_TOKEN, active_msg.GetToken()) == 0) {					
					SendActiveAck(cid, active_msg.GetUid(), active_msg.GetCSeq());
				}
			}
		}
	}
}