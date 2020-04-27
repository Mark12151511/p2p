// PHZ
// 2020-4-15

#ifndef RTP_PACKET_HPP
#define RTP_PACKET_HPP

#include <cstdint>
#include <memory>
#include "asio/asio.hpp"

static const int RTP_HEADER_SIZE = 12;
static const int MAX_RTP_PAYLOAD_SIZE = 1420; //1460  1500-20-12-8
static const int RTP_VERSION = 2;

struct RtpHeader
{
	uint8_t csrc : 4;
	uint8_t extension : 1;
	uint8_t padding : 1;
	uint8_t version : 2;
	uint8_t payload : 7;
	uint8_t marker : 1;

	uint16_t seq;
	uint32_t timestamp;
	uint32_t ssrc;
};

class RtpPacket
{
public:
	RtpPacket& operator=(const RtpPacket&) = delete;
	RtpPacket(const RtpPacket&) = delete;
	RtpPacket()
		: payload_size_(0)
		, max_packet_size_(kMaxPacketSize)
		, packet_(new uint8_t[kMaxPacketSize])
	{
		rtp_header_ = (RtpHeader*)packet_.get();
		memset(rtp_header_, 0, sizeof(RtpHeader));

		rtp_header_->version = RTP_VERSION;
	}

	virtual ~RtpPacket() {}

	uint8_t* Get() const
	{ return packet_.get(); }

	uint32_t Size() const 
	{ return RTP_HEADER_SIZE + payload_size_; }

	void SetRtpHeader(uint8_t* data, uint8_t size) 
	{ 
		if (size == RTP_HEADER_SIZE) {
			memcpy(rtp_header_, data, size);
		}
	}

	void SetPayload(uint8_t* data, uint32_t size) 
	{
		if (MAX_RTP_PAYLOAD_SIZE >= size) {
			payload_size_ = size;
			memcpy(packet_.get() + RTP_HEADER_SIZE, data, size);
		}
	}

	void SetCSRC(uint8_t csrc) 
	{ rtp_header_->csrc = csrc; }

	void SetExtension(uint8_t extension) 
	{ rtp_header_->extension = extension; }

	void SetPadding(uint8_t padding) 
	{ rtp_header_->padding = padding; }

	void SetVersion(uint8_t version) 
	{ rtp_header_->version = version; }

	void SetPayloadType(uint8_t payload_type) 
	{ rtp_header_->payload = payload_type; }

	void SetMarker(uint8_t marker) 
	{ rtp_header_->marker = marker; }

	void SetSeq(uint16_t seq) 
	{ rtp_header_->seq = htons(seq); }

	void SetTimestamp(uint32_t timestamp) 
	{ rtp_header_->timestamp = htonl(timestamp); }

	void SetSSRC(uint32_t ssrc) 
	{ rtp_header_->ssrc = htonl(ssrc); }

	uint32_t GetPayload(uint8_t* buffer, uint32_t buffer_size) const 
	{
		if (payload_size_ > 0) {
			uint32_t copy_size = buffer_size >= payload_size_ ? payload_size_ : buffer_size;
			memcpy(buffer, packet_.get() + RTP_HEADER_SIZE, copy_size);
			return payload_size_;
		}

		return 0;
	}

	uint8_t  GetCSRC() const 
	{ return rtp_header_->csrc; }

	uint8_t  GetExtension() const 
	{ return rtp_header_->extension; }

	uint8_t  GetPadding() const 
	{ return rtp_header_->padding; }

	uint8_t  GetVersion() const 
	{ return rtp_header_->version; }

	uint8_t  GetPayloadType() const 
	{ return rtp_header_->payload; }

	uint8_t  GetMarker() const 
	{ return rtp_header_->marker; }

	uint16_t GetSeq() const 
	{ return ntohs(rtp_header_->seq); }

	uint32_t GetTimestamp() const 
	{ return ntohl(rtp_header_->timestamp); }

	uint32_t GetSSRC() const 
	{ return ntohl(rtp_header_->ssrc); }

private:
	RtpHeader* rtp_header_;
	std::shared_ptr<uint8_t> packet_;
	uint32_t max_packet_size_;
	uint32_t payload_size_;

	static const int kMaxPacketSize = RTP_HEADER_SIZE + MAX_RTP_PAYLOAD_SIZE;
};

#endif
