#include <cstdio>
#include <cstdlib>
#include <iostream>
#include "asio.hpp"
#include "ByteArray.hpp"

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

int main(int argc, char* argv[])
{
	try {

		asio::io_context io_context;
		io_context.run();
	}
	catch (std::exception& e) {
		std::cerr << "Exception: " << e.what() << "\n";
	}

	getchar();
	return 0;
}

#endif