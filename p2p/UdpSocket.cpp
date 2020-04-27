#include "UdpSocket.h"

using namespace asio;

UdpSocket::UdpSocket(asio::io_context& io_context)
	: io_strand_(io_context)
	, data_(new uint8_t[kMaxDataLength])
{

}

UdpSocket::~UdpSocket()
{
	Close();
}

bool UdpSocket::Open(std::string ip, uint16_t port)
{
	ip::udp::endpoint endpoint(ip::address_v4::from_string(ip), port);
	socket_.reset(new ip::udp::socket(io_strand_));
	socket_->open(endpoint.protocol());
	
	error_code ec;
	socket_->bind(endpoint, ec);
	if (ec) {
		socket_ = nullptr;
		return false;
	}

	loacal_endpoint_ = socket_->local_endpoint();
	return true;
}

void UdpSocket::Close()
{
	if (socket_ != nullptr) {
		socket_->close();
		socket_ = nullptr;
	}
}

bool UdpSocket::Send(void* data, size_t size, ip::udp::endpoint remote_endpoint)
{
	if (!socket_) {
		return false;
	}

	socket_->send_to(buffer(data, size), remote_endpoint);
	return true;
}

bool UdpSocket::Receive(std::function<bool(void*, size_t, asio::ip::udp::endpoint&)> cb)
{
	if (!socket_) {
		return false;
	}

	socket_->async_receive_from(asio::buffer(data_.get(), kMaxDataLength), remote_endpoint_,
		io_strand_.wrap([this, cb](std::error_code ec, std::size_t bytes_recvd) {
			if (!ec) {
				if (cb) {
					if (cb(data_.get(), bytes_recvd, remote_endpoint_)) {
						Receive(cb);
					}
				}
			}
		}));

	return true;
}