// PHZ
// 2020-4-15

#ifndef RTP_PACKET_H
#define RTP_PACKET_H

#include <cstdint>
#include <memory>

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
	RtpPacket();
	virtual ~RtpPacket();

	inline void SetRtpHeader(uint8_t* data, uint8_t size);
	inline void SetPayload(uint8_t* data, uint32_t size);
	inline void SetCSRC(uint8_t csrc);
	inline void SetExtension(uint8_t extension);
	inline void SetPadding(uint8_t padding);
	inline void SetVersion(uint8_t version);
	inline void SetPayloadType(uint8_t payload_type);
	inline void SetMarker(uint8_t marker);
	inline void SetSeq(uint16_t seq);
	inline void SetTimestamp(uint32_t timestamp);
	inline void SetSSRC(uint32_t ssrc);

	inline uint32_t GetPayload(uint8_t* buffer, uint32_t buffer_size) const;
	inline uint8_t  GetCSRC() const;
	inline uint8_t  GetExtension() const;
	inline uint8_t  GetPadding() const;
	inline uint8_t  GetVersion() const;
	inline uint8_t  GetPayloadType() const;
	inline uint8_t  GetMarker() const;
	inline uint16_t GetSeq() const;
	inline uint32_t GetTimestamp() const;
	inline uint32_t GetSSRC() const;

private:
	RtpHeader rtp_header_;

	std::shared_ptr<uint8_t> payload_;
	uint32_t payload_size_;
};

#endif