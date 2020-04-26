#include "catch2/catch.hpp"
#include "../UdpSocket.h"
 
TEST_CASE("Test udp socket")
{
	asio::io_context io_context;
	UdpSocket sender_socket(io_context);
	UdpSocket receiver_socket(io_context);

	bool ret = false;

	ret = sender_socket.Open("127.0.0.1", 17675);
	REQUIRE(ret == true);

	ret = receiver_socket.Open("127.0.0.1", 17676);
	REQUIRE(ret == true);

	char req[] = { "hello" };
	char res[] = { "hi" };

	sender_socket.Send(req, sizeof(req), receiver_socket.GetLocalPoint());
	sender_socket.Receive([&sender_socket, &res](void* data, size_t size, asio::ip::udp::endpoint& remote) {
		REQUIRE(strcmp(res, (const char*)data) == 0);
		return false; /* stop receive */
	});

	receiver_socket.Receive([&receiver_socket, &req, &res](void* data, size_t size, asio::ip::udp::endpoint& remote) {
		REQUIRE(strcmp(req, (const char*)data) == 0);
		receiver_socket.Send(res, sizeof(res), remote);
		return false; /* stop receive */
	});

	io_context.run();
}