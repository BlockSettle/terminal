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

bool SingleShotTimer::onActivateExternal(uint64_t expireTime)
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

void SingleShotTimer::onDeactivateExternal()
{
   isActive_ = false;
}

bool SingleShotTimer::onExpireExternal()
{
   if (!isActive_) {
      logger_->error("[SingleShotTimer::onExpireExternal] {} timer is not active to expire!"
         , timerName_);
      return false;
   }

   isActive_ = false;
   expireCallback_();

   return true;
}

std::string SingleShotTimer::GetTimerName() const
{
   return timerName_;
}

uint64_t SingleShotTimer::GetExpireTime() const
{
   return expireTimestamp_;
}

bool SingleShotTimer::IsActive() const
{
   return isActive_;
}