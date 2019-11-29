/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef FUTURE_VALUE_H
#define FUTURE_VALUE_H

#include <atomic>
#include <condition_variable>
#include <mutex>

template<class T>
class FutureValue
{
public:
   FutureValue() = default;
   ~FutureValue() = default;

   FutureValue(const FutureValue&) = delete;
   FutureValue& operator = (const FutureValue&) = delete;
   FutureValue(FutureValue&&) = delete;
   FutureValue& operator = (FutureValue&&) = delete;

   bool setValue(const T& value)
   {
      std::unique_lock<std::mutex> locker(mutex_);
      if (readyFlag_) {
         return false;
      }

      value_ = value;
      readyFlag_ = true;
      event_.notify_all();
      return true;
   }

   const T& waitValue()
   {
      std::unique_lock<std::mutex> locker(mutex_);
      event_.wait(locker, [this]() {
         return readyFlag_;
      });
      return value_;
   }
private:
   std::condition_variable event_;
   mutable std::mutex mutex_;
   bool readyFlag_{false};
   T value_;
};

#endif
