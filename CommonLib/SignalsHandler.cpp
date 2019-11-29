/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
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

