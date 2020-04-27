#include "RtpSink.h"
#include <random>

using namespace asio;

RtpSink::RtpSink(asio::io_service& io_service)
	: io_context_(io_service)
	, io_strand_(io_service)
	, rtp_packet_(new RtpPacket)
{
	std::random_device rd;
	rtp_packet_->SetCSRC(rd());
}

RtpSink::~RtpSink()
{
	Close();
}

bool RtpSink::Open(uint16_t rtp_port, uint16_t rtcp_port)
{
	rtp_socket_.reset(new UdpSocket(io_context_));
	rtp_socket_.reset(new UdpSocket(io_context_));

	if (!rtp_socket_->Open("0.0.0.0", rtp_port) ||
		!rtcp_socket_->Open("0.0.0.0", rtp_port)) {
		rtp_socket_.reset();
		rtcp_socket_.reset();
		return false;
	}

	return true;
}

bool RtpSink::Open()
{
	std::random_device rd;
	for (int n = 0; n <= 10; n++) {
		if (n == 10) {
			return false;
		}

		uint16_t rtp_port  = rd() & 0xfffe;
		uint16_t rtcp_port = rtp_port + 1;

		if (Open(rtp_port, rtcp_port)) {
			break;
		}
	}
	return true;
}

void RtpSink::Close()
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

void RtpSink::SetPeerRtpAddress(std::string ip, uint16_t port)
{
	peer_rtp_address_.size();
	peer_rtp_address_ = ip::udp::endpoint(ip::address_v4::from_string(ip), port);
}

void RtpSink::SetPeerRtcpAddress(std::string ip, uint16_t port)
{
	peer_rtcp_address_ = ip::udp::endpoint(ip::address_v4::from_string(ip), port);
}

uint16_t RtpSink::GetRtpPort() const 
{
	if (rtp_socket_) {
		return rtp_socket_->GetLocalPoint().port();
	}

	return 0;
}

uint16_t RtpSink::GetRtcpPort() const
{
	if (rtcp_socket_) {
		return rtp_socket_->GetLocalPoint().port();
	}

	return 0;
}

bool RtpSink::SendVideo(std::shared_ptr<uint8_t> data, uint32_t size)
{
	if (!rtp_socket_) {
		return false;
	}

	std::weak_ptr<RtpSink> rtp_sink = shared_from_this();
	io_strand_.post([rtp_sink, data, size] {
		auto sink = rtp_sink.lock();
		if (sink) {
			sink->HandleVideo(data, size);
		}		
	});

	return true;
}

void RtpSink::HandleVideo(std::shared_ptr<uint8_t> data, uint32_t size)
{
	if (!rtp_socket_) {
		return;
	}

	int rtp_payload_size = packet_size_ - RTP_HEADER_SIZE;
	int data_index = 0;
	int data_size = size;

	//while (data_index < data_size) {

	//}
}