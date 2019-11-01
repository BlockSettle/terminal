#ifndef THREAD_NAME_H
#define THREAD_NAME_H

#include <string>

namespace bs {

   // Sets name that is visible in crash dumps and in debugger
   // Implemented for Linux and macOS only for now
   // max length for name is 16 symbols including trailing zero (will be trimmed otherwise)
   void setCurrentThreadName(std::string name);

}

#endif
