/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "ManualResetEvent.h"

#include <chrono>

ManualResetEvent::ManualResetEvent()
: eventFlag_(false)
{}

bool ManualResetEvent::WaitForEvent(std::chrono::milliseconds period)
{
   std::unique_lock<std::mutex>  locker(flagMutex_);
   return event_.wait_for(locker, period, [this] () {
      return eventFlag_.load();
   });
}

void ManualResetEvent::WaitForEvent()
{
   std::unique_lock<std::mutex>  locker(flagMutex_);
   event_.wait(locker,
      [this] () { return eventFlag_.load(); }
      );
}

void ManualResetEvent::SetEvent()
{
   std::unique_lock<std::mutex>  locker(flagMutex_);
   if (!eventFlag_) {
      eventFlag_ = true;
      event_.notify_all();
   }
}

void ManualResetEvent::ResetEvent()
{
   std::unique_lock<std::mutex>  locker(flagMutex_);
   eventFlag_ = false;
}
