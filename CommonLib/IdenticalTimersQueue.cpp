#include "IdenticalTimersQueue.h"

#include "FastLock.h"
#include "SingleShotTimer.h"

#include <chrono>

IdenticalTimersQueue::IdenticalTimersQueue(const std::shared_ptr<spdlog::logger>& logger, std::chrono::milliseconds interval)
   : logger_{logger}
   , interval_{interval}
{
   threadActive_ = true;
   waitingThread_ = std::thread(&IdenticalTimersQueue::waitingThreadRoutine, this);
}

IdenticalTimersQueue::~IdenticalTimersQueue() noexcept
{
   stopWaitingThread();
}

std::shared_ptr<SingleShotTimer> IdenticalTimersQueue::CreateTimer(const std::function<void()>& expireCallback
      , const std::string& debugName)
{
   return std::make_shared<SingleShotTimer>(logger_, expireCallback, debugName);
}

std::chrono::steady_clock::time_point IdenticalTimersQueue::GetCurrentTime() const
{
   return std::chrono::steady_clock::now();
}

bool IdenticalTimersQueue::ActivateTimer(const std::shared_ptr<SingleShotTimer>& timer)
{
   if (timer->IsActive()) {
      logger_->error("[IdenticalTimersQueue::ActivateTimer] timer {} is already active"
         , timer->GetTimerName());
      return false;
   }

   const auto expireTime = GetCurrentTime() + interval_;
   if (!timer->onActivateExternal(expireTime)) {
      logger_->debug("[IdenticalTimersQueue::ActivateTimer] failed to activate timer");
      return false;
   }

   bool notifyThread = false;

   {
      FastLock locker{timersQueueLock_};
      notifyThread = activeTimers_.empty();
      activeTimers_.emplace_back(timer);
   }

   if (notifyThread) {
      logger_->debug("[IdenticalTimersQueue::ActivateTimer] activating wait thread");
      timersQueueChanged_.SetEvent();
   }

   return true;
}

bool IdenticalTimersQueue::StopTimer(const std::shared_ptr<SingleShotTimer>& timer)
{
   timer->onDeactivateExternal();
   return true;
}

void IdenticalTimersQueue::waitingThreadRoutine()
{
   while (threadActive_) {
      std::shared_ptr<SingleShotTimer> currentTimer = nullptr;

      {
         FastLock locker{timersQueueLock_};
         if (!activeTimers_.empty()) {
            currentTimer = activeTimers_.front();
         }
      }

      if (currentTimer != nullptr) {
         if (!currentTimer->IsActive()) {
            // skip deactivated timer
            {
               FastLock locker{timersQueueLock_};
               activeTimers_.pop_front();
            }
            continue;
         }

         const auto currentTime = GetCurrentTime();

         if (currentTime < currentTimer->GetExpireTime()) {
            const auto waitTime = std::chrono::duration_cast<std::chrono::milliseconds>(currentTimer->GetExpireTime() - currentTime);
            if (timersQueueChanged_.WaitForEvent(waitTime)) {
               // if activated at this point, this should mean stop
               if (!threadActive_) {
                  break;
               }

               logger_->error("[IdenticalTimersQueue::waitingThreadRoutine] something went wrong. should not be activated!");
               // and break any way
               break;
            }
         }

         if (currentTimer->IsActive()) {
            currentTimer->onExpireExternal();
         }

         {
            FastLock locker{timersQueueLock_};
            activeTimers_.pop_front();
         }

      } else {
         timersQueueChanged_.WaitForEvent();
         timersQueueChanged_.ResetEvent();
      }
   }
}

void IdenticalTimersQueue::stopWaitingThread()
{
   threadActive_ = false;
   timersQueueChanged_.SetEvent();
   waitingThread_.join();
}
