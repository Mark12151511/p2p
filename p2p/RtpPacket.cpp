#include "RtpPacket.h"

RtpPacket::RtpPacket()
	: payload_size_(0)
{
	memset(&rtp_header_, 0, sizeof(RtpHeader));
}

RtpPacket::~RtpPacket()
{

}

inline void RtpPacket::SetRtpHeader(uint8_t* data, uint8_t size)
{
	memcpy(&rtp_header_, data, size);
}

inline void RtpPacket::SetPayload(uint8_t* data, uint32_t size)
{
	if (payload_size_ < size) {
		payload_.reset(new uint8_t[size]);
	}

	payload_size_ = size;
	memcpy(payload_.get(), data, size);
}

inline void RtpPacket::SetCSRC(uint8_t csrc)
{
	rtp_header_.csrc = csrc;
}

inline void RtpPacket::SetExtension(uint8_t extension)
{
	rtp_header_.extension = extension;
}

inline void RtpPacket::SetPadding(uint8_t padding)
{
	rtp_header_.padding = padding;
}

inline void RtpPacket::SetVersion(uint8_t version)
{
	rtp_header_.version = version;
}

inline void RtpPacket::SetPayloadType(uint8_t payload_type)
{
	rtp_header_.payload = payload_type;
}

inline void RtpPacket::SetMarker(uint8_t marker)
{
	rtp_header_.marker = marker;
}

inline void RtpPacket::SetSeq(uint16_t seq)
{
	rtp_header_.seq = seq;
}

inline void RtpPacket::SetTimestamp(uint32_t timestamp)
{
	rtp_header_.timestamp = timestamp;
}

inline void RtpPacket::SetSSRC(uint32_t ssrc)
{
	rtp_header_.ssrc = ssrc;
}

inline uint32_t RtpPacket::GetPayload(uint8_t* buffer, uint32_t buffer_size) const
{
	if (payload_size_ > 0) {
		uint32_t copy_size = buffer_size >= payload_size_ ? payload_size_ : buffer_size;
		memcpy(buffer, payload_.get(), copy_size);
		return payload_size_;
	}

	return 0;
}

inline uint8_t RtpPacket::GetCSRC() const
{
	return rtp_header_.csrc;
}

inline uint8_t RtpPacket::GetExtension()const
{
	return rtp_header_.extension;
}

inline uint8_t RtpPacket::GetPadding()const
{
	return rtp_header_.padding;
}

inline uint8_t RtpPacket::GetVersion()const
{
	return rtp_header_.version;
}

inline uint8_t RtpPacket::GetPayloadType()const
{
	return rtp_header_.payload;
}

inline uint8_t RtpPacket::GetMarker()const
{
	return rtp_header_.marker;
}

inline uint16_t RtpPacket::GetSeq()const
{
	return rtp_header_.seq;
}

inline uint32_t RtpPacket::GetTimestamp()const
{
	return rtp_header_.timestamp;
}

inline uint32_t RtpPacket::GetSSRC()const
{
	return rtp_header_.ssrc;
}

