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

   std::chrono::steady_clock::time_point GetExpireTime() const;
   bool     IsActive() const;
   std::string GetTimerName() const;

private:
   // should be acessed by IdenticalTimersQueue only
   bool onActivateExternal(std::chrono::steady_clock::time_point expireTime);
   void onDeactivateExternal();
   bool onExpireExternal();

private:
   std::shared_ptr<spdlog::logger> logger_;
   std::function<void()>   expireCallback_;
   const std::string       timerName_;

   std::atomic<bool>       isActive_;

   std::chrono::steady_clock::time_point expireTimestamp_{};
};

#endif // __SINGLE_SHOT_TIMER_H__
