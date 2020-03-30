#ifndef FEC_CODEC_H_
#define FEC_CODEC_H_

#include <cstdint>
#include <memory>
#include <map>

namespace fec
{

static const int MAX_PACKET_PAYLOAD_SIZE = 1300;

struct FecHeader
{
	uint64_t fec_timestamp;
	uint32_t fec_index;	
	uint32_t fec_percentage;
	uint32_t fec_payload_size;

	uint32_t total_size;
};

struct FecPacket
{
	FecHeader header;
	uint8_t   payload[MAX_PACKET_PAYLOAD_SIZE];
};

typedef std::map<uint32_t, std::shared_ptr<FecPacket>> FecPackets;

class FecCodec
{
public:
	FecCodec();
	virtual ~FecCodec();
	virtual int set_fec_percentage(int percentage);

protected:
	int _fec_percentage = 5;
};

class FecEncoder : public FecCodec
{
public:
	FecEncoder();
	virtual ~FecEncoder();

	int encode(uint8_t *in_data, uint32_t in_size, FecPackets& out_packets);
};

class FecDecoder : public FecCodec
{
public:
	int decode(FecPackets& in_packets, uint8_t *out_buf, uint32_t max_out_buf_size);
};

}


#endif