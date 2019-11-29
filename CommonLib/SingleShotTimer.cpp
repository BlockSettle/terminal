/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "SingleShotTimer.h"

SingleShotTimer::SingleShotTimer(const std::shared_ptr<spdlog::logger>& logger
      , const std::function<void()>& expireCallback
      , const std::string& debugName)
   : logger_{logger}
   , expireCallback_{expireCallback}
   , timerName_{debugName}
   , isActive_{false}
{
}

SingleShotTimer::~SingleShotTimer() noexcept
{
   isActive_ = false;
}

bool SingleShotTimer::onActivateExternal(std::chrono::steady_clock::time_point expireTime)
{
   if (isActive_) {
      logger_->error("[SingleShotTimer::onActivateExternal] {} timer already active"
         , timerName_);
      return false;
   }

   isActive_ = true;
   expireTimestamp_ = expireTime;

   return true;
}

bool SingleShotTimer::onDeactivateExternal()
{
   bool expected = true;
   constexpr bool desired = false;

   if (!std::atomic_compare_exchange_strong(&isActive_, &expected, desired)) {
      // already stopped
      return false;
   }

   return true;
}

bool SingleShotTimer::onExpireExternal()
{
   if (!onDeactivateExternal()) {
      logger_->error("[SingleShotTimer::onExpireExternal] {} timer is not active to expire!"
         , timerName_);
      return false;
   }

   expireCallback_();

   return true;
}

std::string SingleShotTimer::GetTimerName() const
{
   return timerName_;
}

std::chrono::steady_clock::time_point SingleShotTimer::GetExpireTime() const
{
   return expireTimestamp_;
}

bool SingleShotTimer::IsActive() const
{
   return isActive_;
}
