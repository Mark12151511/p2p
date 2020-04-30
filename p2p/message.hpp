#ifndef	XOP_PROTOCOL_H
#define XOP_PROTOCOL_H

#include <cstdint>
#include "ByteArray.hpp"

namespace xop {

static const uint16_t xop_version = 1;

enum MsgType
{
	MIN_MSG_ID = 0x10,

	/* 请求连接 */
	MSG_ACTIVE,
	MSG_ACTIVE_ACK,

	/* 建立会话 */
	MSG_SETUP,
	MSG_SETUP_ACK,

	/* 请求音视频流 */
	MSG_PLAY,
	MSG_PLAY_ACK,

	/* 心跳 */
	MSG_PING,
	MSG_PONG,

	/* 请求关键帧 */
	MSG_FORCE_IDR,

	/* 反馈信息 */
	MSG_FEEDBACK,

	/* 关闭连接 */
	MSG_TEARDOWN,
	MSG_TEARDOWN_ACK,

	MAX_MSG_ID
};

class MsgHeader
{
public:
	MsgHeader() 
	{
		type_ = 0;
		version_ = xop_version;
		uid_ = 0;
		cseq_ = 0;
		timestamp_ = 0;
		error_code_ = 0;
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


	virtual int Encode(ByteArray& byte_array)
	{
		EncodeHeader(byte_array);
		return byte_array.Size();
	}

	virtual int Decode(ByteArray& byte_array)
	{
		DecodeHeader(byte_array);
		return byte_array.Size();
	}

	int GetErrorCode()
	{ return error_code_; }

protected:
	uint8_t  type_;
	uint8_t  version_;
	uint32_t uid_;
	uint32_t cseq_;
	uint32_t timestamp_;
	uint32_t error_code_;
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

	virtual int Encode(ByteArray& byte_array)
	{
		EncodeHeader(byte_array);
		byte_array.WriteUint16LE(token_size_);
		if (token_size_ > 0) {
			byte_array.Write((char*)token_.get(), token_size_);			
		}		

		return byte_array.Size();
	}

	virtual int Decode(ByteArray& byte_array)
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
	ActiveAckMsg(uint32_t error_code = 0)
	{ 
		error_code_ = error_code;
		type_ = MSG_ACTIVE_ACK; 
	}

private:
	uint32_t error_code_;
};

class SetupMsg : public MsgHeader
{
public:
	SetupMsg(){ }

	SetupMsg(uint16_t rtp_port, uint16_t rtcp_port)
		: rtp_port_(rtp_port)
		, rtcp_port_(rtcp_port)
	{
		type_ = MSG_SETUP;
	}

	virtual int Encode(ByteArray& byte_array)
	{
		EncodeHeader(byte_array);
		byte_array.WriteUint16LE(rtp_port_);
		byte_array.WriteUint16LE(rtcp_port_);
		return byte_array.Size();
	}

	virtual int Decode(ByteArray& byte_array)
	{
		DecodeHeader(byte_array);
		if (type_ != MSG_SETUP) {
			return -1;
		}
		rtp_port_ = byte_array.ReadUint16LE();
		rtcp_port_ = byte_array.ReadUint16LE();
		return byte_array.Size();
	}

private:
	uint16_t rtp_port_  = 0;
	uint16_t rtcp_port_ = 0;
};

class SetupAckMsg : public MsgHeader
{
public:
	SetupAckMsg(uint32_t error_code = 0)
	{
		error_code_ = error_code;
		type_ = MSG_SETUP_ACK;
	}
};

}

#endif