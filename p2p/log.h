#ifndef _XOP_LOG_H
#define _XOP_LOG_H

#include <cstdio>

#define LOG(format, ...)  	\
{								\
    fprintf(stderr, "[%s:%d] " format " \n", \
   __FUNCTION__ , __LINE__, ##__VA_ARGS__);     \
}

#endif
