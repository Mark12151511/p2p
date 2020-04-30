#include <cstdio>
#include <cstdlib>
#include <iostream>
#include "asio.hpp"

#define CATCH_CONFIG_RUNNER

#ifdef CATCH_CONFIG_RUNNER

#include "unittest/unittest.hpp"
int main(int argc, char* argv[]) 
{
	Catch::Session session;
	session.run(argc, argv);
	getchar();
	return 0;
}

#else

#include "MediaServer.h"
#include "MediaClient.h"

int main(int argc, char* argv[])
{
	MediaServer server;
	server.Start("0.0.0.0", 17676);
	
	MediaClient client;
	client.Connect("127.0.0.1", 17676);

	while (1)
	{
		if (!client.IsConnected()) {
			break;
		}

		Sleep(1000);
	}

	getchar();
	return 0;
}

#endif