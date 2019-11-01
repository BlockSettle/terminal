#include "ThreadName.h"

#ifdef _WIN32

void bs::setCurrentThreadName(const char *name)
{
}

#else

#include <pthread.h>

void bs::setCurrentThreadName(std::string name)
{
   const size_t maxNameLen = 15;
   if (name.size() > maxNameLen) {
      name.resize(maxNameLen);
   }

   pthread_setname_np(pthread_self(), name.c_str());
}

#endif
