#include <cstdio>

//#define CATCH_CONFIG_MAIN
#define CATCH_CONFIG_RUNNER
#include "unittest/unittest.hpp"

int main(int argc, char* argv[]) 
{
#ifdef CATCH_CONFIG_RUNNER

	Catch::Session session;
	session.run(argc, argv);

#else

#endif
	getchar();
	return 0;
}
