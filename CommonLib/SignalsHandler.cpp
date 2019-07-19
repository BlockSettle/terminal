#include "SignalsHandler.h"

#ifdef _WIN32

void SignalsHandler::registerHandler(const Cb &cb, const std::initializer_list<int> &signalsList)
{
}

#else // _WIN32

#include <thread>
#include <signal.h>

void SignalsHandler::registerHandler(const Cb &cb, const std::initializer_list<int> &signalsList)
{
   sigset_t set;

   sigemptyset(&set);
   for (int sig : signalsList)  {
      sigaddset(&set, sig);
   }

   sigprocmask(SIG_BLOCK, &set, nullptr);

   auto cbWrap = [set, cb] {
      while (true) {
         int sig = 0;
         sigwait(&set, &sig);
         cb(sig);
      }
   };

   std::thread(cbWrap).detach();
}

#endif // _WIN32

