#include "ThreadName.h"

#ifdef Q_OS_LINUX

#include <pthread.h>

void bs::setCurrentThreadName(std::string name)
{
   const size_t maxNameLen = 15;
   if (name.size() > maxNameLen) {
      name.resize(maxNameLen);
   }

   pthread_setname_np(pthread_self(), name.c_str());
}

#else

void bs::setCurrentThreadName(std::string name)
{
}

#endif
