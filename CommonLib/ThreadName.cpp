#include "ThreadName.h"

#ifdef _WIN32

void bs::setCurrentThreadName(const char *name)
{
}

#else

#include <pthread.h>

void bs::setCurrentThreadName(const std::string &name)
{
   pthread_setname_np(pthread_self(), name.c_str());
}

#endif
