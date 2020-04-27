#include "RtpPacket.h"
#include "asio/asio.hpp"

RtpPacket::RtpPacket()
	: payload_size_(0)
	, max_packet_size_(kMaxPacketSize)
	, packet_(new uint8_t[kMaxPacketSize])
{
	rtp_header_ = (RtpHeader*)packet_.get();
	memset(&rtp_header_, 0, sizeof(RtpHeader));

	rtp_header_->version = RTP_VERSION;
}

RtpPacket::~RtpPacket()
{

}

uint8_t* RtpPacket::Get()
{
	return packet_.get();
}

uint32_t RtpPacket::Size()
{
	return RTP_HEADER_SIZE + payload_size_;
}

void RtpPacket::SetRtpHeader(uint8_t* data, uint8_t size)
{
	memcpy(rtp_header_, data, size);
}

void RtpPacket::SetPayload(uint8_t* data, uint32_t size)
{
	if (MAX_RTP_PAYLOAD_SIZE < size) {
		return;
	}

	payload_size_ = size;
	memcpy(packet_.get() + RTP_HEADER_SIZE, data, size);
}

void RtpPacket::SetCSRC(uint8_t csrc)
{
	rtp_header_->csrc = csrc;
}

void RtpPacket::SetExtension(uint8_t extension)
{
	rtp_header_->extension = extension;
}

void RtpPacket::SetPadding(uint8_t padding)
{
	rtp_header_->padding = padding;
}

void RtpPacket::SetVersion(uint8_t version)
{
	rtp_header_->version = version;
}

void RtpPacket::SetPayloadType(uint8_t payload_type)
{
	rtp_header_->payload = payload_type;
}

void RtpPacket::SetMarker(uint8_t marker)
{
	rtp_header_->marker = marker;
}

void RtpPacket::SetSeq(uint16_t seq)
{
	rtp_header_->seq = htons(seq); 
}

void RtpPacket::SetTimestamp(uint32_t timestamp)
{
	rtp_header_->timestamp = htonl(timestamp);
}

void RtpPacket::SetSSRC(uint32_t ssrc)
{
	rtp_header_->ssrc = htonl(ssrc);
}

uint32_t RtpPacket::GetPayload(uint8_t* buffer, uint32_t buffer_size) const
{
	if (payload_size_ > 0) {
		uint32_t copy_size = buffer_size >= payload_size_ ? payload_size_ : buffer_size;
		memcpy(buffer, packet_.get() + RTP_HEADER_SIZE, copy_size);
		return payload_size_;
	}

	return 0;
}

uint8_t RtpPacket::GetCSRC() const
{
	return rtp_header_->csrc;
}

uint8_t RtpPacket::GetExtension()const
{
	return rtp_header_->extension;
}

uint8_t RtpPacket::GetPadding()const
{
	return rtp_header_->padding;
}

uint8_t RtpPacket::GetVersion()const
{
	return rtp_header_->version;
}

uint8_t RtpPacket::GetPayloadType()const
{
	return rtp_header_->payload;
}

uint8_t RtpPacket::GetMarker()const
{
	return rtp_header_->marker;
}

uint16_t RtpPacket::GetSeq()const
{
	return ntohs(rtp_header_->seq);
}

uint32_t RtpPacket::GetTimestamp()const
{
	return ntohl(rtp_header_->timestamp);
}

uint32_t RtpPacket::GetSSRC()const
{
	return ntohl(rtp_header_->ssrc);
}

