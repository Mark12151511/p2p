#include "RtpSink.h"
#include <random>
#include <chrono>

using namespace asio;
using namespace std::chrono;

static inline uint32_t GetTimestamp()
{
	auto time_point = time_point_cast<milliseconds>(high_resolution_clock::now());
	return (uint32_t)time_point.time_since_epoch().count();
}

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
	rtcp_socket_.reset(new UdpSocket(io_context_));

	if (!rtp_socket_->Open("0.0.0.0", rtp_port) ||
		!rtcp_socket_->Open("0.0.0.0", rtcp_port)) {
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
		return rtcp_socket_->GetLocalPoint().port();
	}

	return 0;
}

bool RtpSink::SendVideo(std::shared_ptr<uint8_t> data, uint32_t size)
{
	if (!rtp_socket_) {
		return false;
	}

	std::weak_ptr<RtpSink> rtp_sink = shared_from_this();

	io_strand_.dispatch([rtp_sink, data, size] {
		auto sink = rtp_sink.lock();
		if (sink) {
			sink->HandleVideo(data, size);
		}		
	});

	return true;
}

void RtpSink::HandleVideo(std::shared_ptr<uint8_t> data, uint32_t size)
{
	if (!rtp_socket_ || !peer_rtp_address_.port()) {
		return;
	}

	int rtp_payload_size = packet_size_ - RTP_HEADER_SIZE;
	int data_index = 0;
	int data_size = size;

	rtp_packet_->SetPayloadType(1);
	rtp_packet_->SetTimestamp(GetTimestamp());
	rtp_packet_->SetMarker(0);

	while (data_index < data_size) {
		int bytes_used = data_size - data_index;
		if (bytes_used > rtp_payload_size) {
			bytes_used = rtp_payload_size;			
		}
		else {
			rtp_packet_->SetMarker(1);
		}

		rtp_packet_->SetSeq(packet_seq_++);
		rtp_packet_->SetPayload(data.get() + data_index, bytes_used);
		data_index += bytes_used;

		rtp_socket_->Send(rtp_packet_->Get(), rtp_packet_->Size(), peer_rtp_address_);
		//printf("mark:%d, seq:%d, size: %u\n", rtp_packet_->GetMarker(), rtp_packet_->GetSeq(), rtp_packet_->Size());
	}
}