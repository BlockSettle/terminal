#ifndef __SINGLE_SHOT_TIMER_H__
#define __SINGLE_SHOT_TIMER_H__

#include <atomic>
#include <functional>

#include <spdlog/spdlog.h>

class IdenticalTimersQueue;

class SingleShotTimer
{
friend class IdenticalTimersQueue;
public:
   SingleShotTimer(const std::shared_ptr<spdlog::logger>& logger
      , const std::function<void()>& expireCallback
      , const std::string& debugName);

   ~SingleShotTimer() noexcept;

   SingleShotTimer(const SingleShotTimer&) = delete;
   SingleShotTimer& operator = (const SingleShotTimer&) = delete;

   SingleShotTimer(SingleShotTimer&&) = delete;
   SingleShotTimer& operator = (SingleShotTimer&&) = delete;

   uint64_t GetExpireTime() const;
   bool     IsActive() const;
   std::string GetTimerName() const;

private:
   // should be acessed by IdenticalTimersQueue only
   bool onActivateExternal(uint64_t expireTime);
   void onDeactivateExternal();
   bool onExpireExternal();

private:
   std::shared_ptr<spdlog::logger> logger_;
   std::function<void()>   expireCallback_;
   const std::string       timerName_;

   std::atomic<bool>       isActive_;

   uint64_t                expireTimestamp_ = 0;
};

#endif // __SINGLE_SHOT_TIMER_H__