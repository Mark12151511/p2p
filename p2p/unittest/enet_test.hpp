#include "catch2/catch.hpp"
#include "../ENetClient.h"
#include "../ENetServer.h"
#include <random>
#include <memory>
#include <thread>

TEST_CASE("Test enet")
{
	ENetServer server;
	ENetClient client;

	int ret = enet_initialize();
	REQUIRE(ret == 0);

	std::thread server_thread([&server] {
		bool ret = server.Start("0.0.0.0", 17676);
		REQUIRE(ret == true);

		uint32_t cid = 0;
		uint8_t req[1024];
		int req_size = 0;
		memset(req, 0, 1024);
		
		for (int i = 0; i < 3; i++) {
			req_size = server.Recv(&cid, req, 1024, 1000);
			if (req_size > 0) {
				REQUIRE(req_size == (strlen("hello") + 1));

				uint8_t res[] = "hi";
				server.Send(cid, res, sizeof(res));
				break;
			}			
		}			

		REQUIRE(req_size > 0);
	});

	std::thread client_thread([&client] {
		bool ret = client.Connect("127.0.0.1", 17676, 1000);
		REQUIRE(ret == true);

		uint8_t req[] = "hello";
		client.Send(req, sizeof(req));

		uint8_t res[] = "hello";
		int res_size = client.Recv(res, sizeof(res), 100);
		if (res_size > 0) {
			REQUIRE(res_size == (strlen("hi") + 1));
		}
		REQUIRE(res_size > 0);
	});

	
	server_thread.join();
	client_thread.join();

	enet_deinitialize();
}