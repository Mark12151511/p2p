#ifndef RTP_SOURCE_H
#define RTP_SOURCE_H

#include "UdpSocket.h"
#include "RtpPacket.hpp"
#include <map>

class RtpSource : public std::enable_shared_from_this<RtpSource>
{
public:
	typedef std::function<bool(std::shared_ptr<uint8_t> data, size_t size, uint8_t type, uint32_t timestamp)> MediaCB;

	RtpSource& operator=(const RtpSource&) = delete;
	RtpSource(const RtpSource&) = delete;
	RtpSource(asio::io_service& io_service);
	virtual ~RtpSource();

	bool Open(uint16_t rtp_port, uint16_t rtcp_port);
	bool Open();
	void Close();

	void SetMediaCB(MediaCB cb) 
	{ media_cb_ = cb; }

	uint16_t GetRtpPort()  const;
	uint16_t GetRtcpPort() const;

private:
	bool OnRead(void* data, size_t size);
	bool OnFrame();

	asio::io_context& io_context_;
	asio::io_context::strand io_strand_;

	std::unique_ptr<UdpSocket> rtp_socket_;
	std::unique_ptr<UdpSocket> rtcp_socket_;
	asio::ip::udp::endpoint peer_rtp_address_;
	asio::ip::udp::endpoint peer_rtcp_address_;

	std::map<int, std::shared_ptr<RtpPacket>> video_packets_;
	std::map<int, std::shared_ptr<RtpPacket>> audio_packets_;

	MediaCB media_cb_;
};

#endif