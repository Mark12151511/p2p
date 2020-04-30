#include "MediaClient.h"
#include "message.hpp"
#include "log.h"

#define TEST_TOKEN "12345"

using namespace xop;

MediaClient::MediaClient()
{
	is_active_ = false;
	is_setup_  = false;
	is_play_   = false;
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

	if (is_started_) {
		Stop();
	}

	if (!event_client_.Connect(ip, port, timeout_msec)) {
		LOG("Event client connect failed.");
		return false;
	}

	is_started_ = true;

	SendActive();
	PollEvent(true, timeout_msec / 2);

	if (!is_active_) {
		event_client_.Close();
		is_started_ = false;
		return false;
	}
	
	if (!Start()) {
		Stop();
		return false;
	}

	SendSetup();
	return true;
}

void MediaClient::Close()
{
	std::lock_guard<std::mutex> locker(mutex_);
	Stop();	
}

bool MediaClient::Start()
{
	if (is_started_) {
		io_service_work_.reset(new asio::io_service::work(io_service_));
		rtp_source_.reset(new RtpSource(io_service_));
		if (!rtp_source_->Open()) {
			LOG("Rtp source open failed.");
			rtp_source_.reset();
			return false;
		}

		if (event_thread_ == nullptr) {
			event_thread_.reset(new std::thread([this] {
				PollEvent();
			}));
		}
	}

	return true;
}

void MediaClient::Stop()
{
	if (is_started_) {
		is_started_ = false;
		if (event_thread_ != nullptr) {
			event_thread_->join();
			event_thread_ = nullptr;
		}

		event_client_.Close();
		io_service_work_.reset();

		is_active_ = false;
		is_setup_ = false;
		is_play_ = false;
	}
}

bool MediaClient::IsConnected()
{
	std::lock_guard<std::mutex> locker(mutex_);
	return is_active_;
}

void MediaClient::PollEvent(bool once, uint32_t timeout_msec)
{
	uint32_t msec = timeout_msec;
	uint32_t cid = 0;
	uint32_t max_message_len = 1500;
	std::shared_ptr<uint8_t> message(new uint8_t[max_message_len]);

	while (is_started_ )
	{
		int msg_len = event_client_.Recv(message.get(), max_message_len, msec);
		if (msg_len > 0) {
			if (!OnMessage((char*)message.get(), msg_len)) {
				is_active_ = false;
				break;
			}
		}
		else if(msg_len < 0) {
			is_active_ = false;
		}

		if (!event_client_.IsConnected()) {
			is_active_ = false;
		}

		if (once) {
			break;
		}
	}
}

bool MediaClient::OnMessage(const char* message, uint32_t len)
{
	int msg_type = message[0];
	ByteArray byte_array(message, len);

	switch (msg_type)
	{
	case MSG_ACTIVE_ACK:
		{
			ActiveAckMsg active_ack_msg;
			active_ack_msg.Decode(byte_array);
			if (active_ack_msg.GetErrorCode()) {
				return false;
			}
			is_active_ = true;
		}
		break;
	default:
		break;
	}

	return true;
}

void MediaClient::SendActive()
{
	if (event_client_.IsConnected()) {
		std::string token = TEST_TOKEN;
		ByteArray byte_array;

		ActiveMsg msg(token.c_str(), (uint32_t)token.size() + 1);
		int size = msg.Encode(byte_array);
		if (size > 0) {
			event_client_.Send(byte_array.Data(), size);
		}
	}
}

void MediaClient::SendSetup()
{
	if (event_client_.IsConnected()) {

		uint16_t rtp_port = 0;
		uint16_t rtcp_port = 0;
		if (rtp_source_) {
			rtp_port = rtp_source_->GetRtpPort();
			rtcp_port = rtp_source_->GetRtcpPort();
		}

		if (rtp_port > 0 && rtcp_port > 0) {
			ByteArray byte_array;
			SetupMsg msg(rtp_port, rtcp_port);
			int size = msg.Encode(byte_array);
			if (size > 0) {
				event_client_.Send(byte_array.Data(), size);
			}
		}
	}
}
