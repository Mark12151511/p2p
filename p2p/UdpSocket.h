#ifndef UDP_SOCKET_H
#define UDP_SOCKET_H

#include <memory>
#include <cstdint>
#include <string>
#include "asio/asio.hpp"

class UdpSocket
{
public:
	UdpSocket(asio::io_context& io_context);
	virtual ~UdpSocket();

	bool Open(std::string ip, uint16_t port);
	void Close();

	bool Send(void* data, size_t size, asio::ip::udp::endpoint remote_endpoint);
	bool Receive(std::function<bool(void*, size_t, asio::ip::udp::endpoint&)> cb);

	asio::ip::udp::endpoint GetLocalPoint() const
	{ return loacal_endpoint_;  }

private:
	asio::io_context::strand io_strand_;
	std::unique_ptr<asio::ip::udp::socket> socket_;
	asio::ip::udp::endpoint loacal_endpoint_;
	asio::ip::udp::endpoint remote_endpoint_;
	std::unique_ptr<uint8_t> data_;

	static const int kMaxDataLength = 2048;
};

#endif