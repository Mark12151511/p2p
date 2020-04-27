#include "RtpSource.h"
#include <random>

using namespace asio;

RtpSource::RtpSource(asio::io_service& io_service)
	: io_context_(io_service)
	, io_strand_(io_service)
{

}

RtpSource::~RtpSource()
{
	Close();
}

bool RtpSource::Open(uint16_t rtp_port, uint16_t rtcp_port)
{
	rtp_socket_.reset(new UdpSocket(io_context_));
	rtcp_socket_.reset(new UdpSocket(io_context_));

	if (!rtp_socket_->Open("0.0.0.0", rtp_port) ||
		!rtcp_socket_->Open("0.0.0.0", rtcp_port)) {
		rtp_socket_.reset();
		rtcp_socket_.reset();
		return false;
	}

	std::weak_ptr<RtpSource> rtp_source = shared_from_this();
	rtp_socket_->Receive([rtp_source](void* data, size_t size, asio::ip::udp::endpoint& ep) {
		auto source = rtp_source.lock();
		if (!source) {
			return false;
		}

		return source->OnRead(data, size);
	});

	return true;
}

bool RtpSource::Open()
{
	std::random_device rd;
	for (int n = 0; n <= 10; n++) {
		if (n == 10) {
			return false;
		}

		uint16_t rtp_port = rd() & 0xfffe;
		uint16_t rtcp_port = rtp_port + 1;

		if (Open(rtp_port, rtcp_port)) {
			break;
		}
	}
	return true;
}

void RtpSource::Close()
{
	if (rtp_socket_) {
		rtp_socket_->Close();
		rtp_socket_.reset();
	}

	if (rtcp_socket_) {
		rtcp_socket_->Close();
		rtcp_socket_.reset();
	}
}

bool RtpSource::OnRead(void* data, size_t size)
{
	if (size < RTP_HEADER_SIZE) {
		return false;
	}

	auto packet = std::make_shared<RtpPacket>();
	packet->SetRtpHeader((uint8_t*)data, RTP_HEADER_SIZE);

	if (size > RTP_HEADER_SIZE) {
		packet->SetPayload((uint8_t*)data + RTP_HEADER_SIZE, size - RTP_HEADER_SIZE);
	}

	uint8_t  mark = packet->GetMarker();
	uint16_t seq = packet->GetSeq();
	//printf("mark:%d, seq:%d, size: %u\n", mark, seq, size);

	video_packets_[seq] = packet;

	if (mark) {
		return OnFrame();
	}

	return true;
}

bool RtpSource::OnFrame()
{
	if (video_packets_.empty()) {
		return false;
	}

	std::shared_ptr<uint8_t> data(new uint8_t[video_packets_.size() * 1500]);
	int data_size = 0;
	int data_index = 0;

	auto first_packet = video_packets_.begin()->second;
	uint32_t timestamp = first_packet->GetTimestamp();
	uint8_t type = first_packet->GetPayloadType();

	for (auto iter : video_packets_) {
		auto packet = iter.second;
		int payload_size = packet->GetPayload(data.get() + data_index, 1500);
		data_index += payload_size;
		data_size += payload_size;
	}

	video_packets_.clear();

	if (data_size > 0 && media_cb_) {
		return media_cb_(data, data_size, type, timestamp);
	}

	return true;
}
