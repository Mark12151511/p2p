#ifndef	XOP_PROTOCOL_H
#define XOP_PROTOCOL_H

#include <cstdint>
#include "ByteArray.hpp"

namespace xop {

static const uint16_t xop_version = 1;

enum MsgType
{
	MIN_MSG_ID = 0x10,

	MSG_ACTIVE,
	MSG_ACTIVE_ACK,
	MSG_PING,
	MSG_PONG,
	MSG_FIR,
	MSG_FEEDBACK,

	MAX_MSG_ID
};

class MsgHeader
{
public:
	uint8_t  type_;
	uint8_t  version_;
	uint32_t uid_;
	uint32_t cseq_;
	uint32_t timestamp_;

	MsgHeader() 
	{
		type_ = 0;
		version_ = xop_version;
		uid_ = 0;
		cseq_ = 0;
		timestamp_ = 0;
	}

	uint8_t GetType() 
	{ return type_; }

	uint32_t GetUid() 
	{ return uid_; }

	uint32_t GetCSeq() 
	{ return cseq_; }

	uint32_t GetTimestamp() 
	{ return timestamp_; }

	void SetType(uint8_t type) 
	{ type_ = type; }

	void SetUid(uint32_t uid) 
	{  uid_ = uid; }

	void SetCSeq(uint32_t cseq) 
	{  cseq_; }

	void SetTimestamp(uint32_t timestamp)
	{  timestamp_ = timestamp; }

	void EncodeHeader(ByteArray& byte_array) 
	{
		byte_array.Write((char*)&type_, 1);
		byte_array.Write((char*)&version_, 1);
		byte_array.WriteUint32BE(uid_);
		byte_array.WriteUint32BE(cseq_);
		byte_array.WriteUint32BE(timestamp_);
	}

	void DecodeHeader(ByteArray& byte_array) 
	{
		byte_array.Read((char*)&type_, 1);
		byte_array.Read((char*)&version_, 1);
		uid_ = byte_array.ReadUint32BE();
		cseq_ = byte_array.ReadUint32BE();
		timestamp_ = byte_array.ReadUint32BE();
	}
};

class ActiveMsg : public MsgHeader
{
public:
	ActiveMsg(const char* token, uint32_t token_size) 
	{
		type_ = MSG_ACTIVE;
		token_size_ = token_size;
		if (token_size_ > 0) {
			token_.reset(new char[token_size_]);
			memcpy(token_.get(), token, token_size);
		}
	}
	 
	ActiveMsg() 
	{
		type_ = MSG_ACTIVE;
		token_size_ = 0;
	}

	int Encode(ByteArray& byte_array) 
	{
		EncodeHeader(byte_array);
		byte_array.WriteUint16LE(token_size_);
		if (token_size_ > 0) {
			byte_array.Write((char*)token_.get(), token_size_);			
		}		

		return byte_array.Size();
	}

	int Decode(ByteArray& byte_array) 
	{
		DecodeHeader(byte_array);
		if (type_ != MSG_ACTIVE) {
			return -1;
		}

		token_size_ = byte_array.ReadUint16LE();
		if (token_size_ > 0) {
			token_.reset(new char[token_size_]);
			byte_array.Read((char*)token_.get(), token_size_);
		}
		
		return byte_array.Size();
	}

	const char* GetToken() 
	{ return token_.get(); }

	uint32_t GetTokenSize() 
	{ return token_size_; }

private:
	std::shared_ptr<char> token_;
	uint32_t token_size_ = 0;
};

class ActiveAckMsg : public MsgHeader
{
public:
	ActiveAckMsg() 
	{ type_ = MSG_ACTIVE_ACK; }

	int Encode(ByteArray& byte_array)
	{
		EncodeHeader(byte_array);
		return byte_array.Size();
	}

	int Decode(ByteArray& byte_array)
	{
		DecodeHeader(byte_array);
		return byte_array.Size();
	}
};

class FirMsg : public MsgHeader
{

private:
	
};

}


#endif