#ifndef RTP_H_
#define RTP_H_

#include <memory>
#include <cstdint>

static const int RTP_HEADER_SIZE = 12;
static const int MAX_RTP_PAYLOAD_SIZE = 1420; //1460  1500-20-12-8
static const int RTP_VERSION = 2;
static const int RTP_TCP_HEAD_SIZE = 4;

struct RtpHeader
{
    uint8_t csrc:4;
	uint8_t extension:1;
	uint8_t padding:1;
	uint8_t version:2;
	uint8_t payload:7;
	uint8_t marker:1;

	uint16_t seq;
	uint32_t ts;
	uint32_t ssrc;
};

struct RtpPacket
{
	RtpHeader header;
	uint8_t payload[MAX_RTP_PAYLOAD_SIZE];
};


#endif
