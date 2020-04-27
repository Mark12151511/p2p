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

	uint8_t* Get();
	uint32_t Size();

	void SetRtpHeader(uint8_t* data, uint8_t size);
	void SetPayload(uint8_t* data, uint32_t size);
	void SetCSRC(uint8_t csrc);
	void SetExtension(uint8_t extension);
	void SetPadding(uint8_t padding);
	void SetVersion(uint8_t version);
	void SetPayloadType(uint8_t payload_type);
	void SetMarker(uint8_t marker);
	void SetSeq(uint16_t seq);
	void SetTimestamp(uint32_t timestamp);
	void SetSSRC(uint32_t ssrc);

	uint32_t GetPayload(uint8_t* buffer, uint32_t buffer_size) const;
	uint8_t  GetCSRC() const;
	uint8_t  GetExtension() const;
	uint8_t  GetPadding() const;
	uint8_t  GetVersion() const;
	uint8_t  GetPayloadType() const;
	uint8_t  GetMarker() const;
	uint16_t GetSeq() const;
	uint32_t GetTimestamp() const;
	uint32_t GetSSRC() const;

private:
	RtpHeader* rtp_header_;

	std::shared_ptr<uint8_t> packet_;
	uint32_t max_packet_size_;
	uint32_t payload_size_;

	static const int kMaxPacketSize = RTP_HEADER_SIZE + MAX_RTP_PAYLOAD_SIZE;
};

#endif