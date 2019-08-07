#ifndef __IDENTICAL_TIMERS_QUEUE_H__
#define __IDENTICAL_TIMERS_QUEUE_H__

#include "ManualResetEvent.h"

#include <atomic>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <thread>

#include <spdlog/spdlog.h>

class SingleShotTimer;

class IdenticalTimersQueue
{
public:
   IdenticalTimersQueue(const std::shared_ptr<spdlog::logger>& logger, std::chrono::milliseconds interval);
   ~IdenticalTimersQueue() noexcept;

   IdenticalTimersQueue(const IdenticalTimersQueue&) = delete;
   IdenticalTimersQueue& operator = (const IdenticalTimersQueue&) = delete;

   IdenticalTimersQueue(IdenticalTimersQueue&&) = delete;
   IdenticalTimersQueue& operator = (IdenticalTimersQueue&&) = delete;

   std::shared_ptr<SingleShotTimer> CreateTimer(const std::function<void()>& expireCallback
      , const std::string& debugName);
   bool ActivateTimer(const std::shared_ptr<SingleShotTimer>& timer);
   bool StopTimer(const std::shared_ptr<SingleShotTimer>& timer);

private:
   void waitingThreadRoutine();

   void stopWaitingThread();

   std::chrono::steady_clock::time_point GetCurrentTime() const;

private:
   std::shared_ptr<spdlog::logger> logger_;
   const std::chrono::milliseconds interval_;

   ManualResetEvent     timersQueueChanged_;

   std::atomic<bool>    threadActive_;

   std::atomic_flag     timersQueueLock_ = ATOMIC_FLAG_INIT;
   std::deque<std::shared_ptr<SingleShotTimer>> activeTimers_;

   std::thread       waitingThread_;
};

#endif // __IDENTICAL_TIMERS_QUEUE_H__
