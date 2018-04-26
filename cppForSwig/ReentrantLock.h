////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//  Copyright (C) 2017, goatpig                                               //
//  Distributed under the MIT license                                         //
//  See LICENSE-MIT or https://opensource.org/licenses/MIT                    //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#ifndef _H_REENTRANT_LOCK
#define _H_REENTRANT_LOCK

using namespace std;

#include <thread>
#include <mutex>
#include <memory>
#include <string>

#include "make_unique.h"

class LockableException : public runtime_error
{
public:
   LockableException(const string& err) : runtime_error(err)
   {}
};

struct AlreadyLocked
{};

////////////////////////////////////////////////////////////////////////////////
class Lockable
{
   friend struct ReentrantLock;
   friend struct SingleLock;

protected:
   mutex mu_;
   thread::id mutexTID_;

public:
   virtual ~Lockable(void) = 0;

   bool ownsLock(void) const
   {
      auto thisthreadid = this_thread::get_id();
      return mutexTID_ == thisthreadid;
   }

   virtual void initAfterLock(void) = 0;
   virtual void cleanUpBeforeUnlock(void) = 0;
};

////////////////////////////////////////////////////////////////////////////////
struct SingleLock
{
private:
   Lockable* lockablePtr_;
   unique_ptr<unique_lock<mutex>> lock_;

private:
   SingleLock(const SingleLock&) = delete;
   SingleLock& operator=(const SingleLock&) = delete;
   SingleLock& operator=(SingleLock&&) = delete;

public:
   SingleLock(const Lockable* ptr)
   {
      lockablePtr_ = const_cast<Lockable*>(ptr);
      if (lockablePtr_ == nullptr)
         throw LockableException("null lockable ptr");

      if (lockablePtr_->mutexTID_ == this_thread::get_id())
         throw AlreadyLocked();

      lock_ =
         make_unique<unique_lock<mutex>>(lockablePtr_->mu_, defer_lock);

      lock_->lock();
      lockablePtr_->mutexTID_ = this_thread::get_id();

      lockablePtr_->initAfterLock();
   }

   SingleLock(SingleLock&& lock) :
      lockablePtr_(lock.lockablePtr_)
   {
      lock_ = move(lock.lock_);
   }

   ~SingleLock(void)
   {
      if (lock_ == nullptr)
         return;

      if (lock_->owns_lock())
      {
         if (lockablePtr_ != nullptr)
         {
            lockablePtr_->mutexTID_ = thread::id();
            lockablePtr_->cleanUpBeforeUnlock();
         }
      }
   }
};

////////////////////////////////////////////////////////////////////////////////
struct ReentrantLock
{
private:
   Lockable* lockablePtr_ = nullptr;
   unique_ptr<unique_lock<mutex>> lock_;

private:
   ReentrantLock(const ReentrantLock&) = delete;
   ReentrantLock& operator=(const ReentrantLock&) = delete;
   ReentrantLock& operator=(ReentrantLock&&) = delete;

public:
   ReentrantLock(const Lockable* ptr)
   {
      lockablePtr_ = const_cast<Lockable*>(ptr);
      if (lockablePtr_ == nullptr)
         throw LockableException("null lockable ptr");
      
      if (lockablePtr_->mutexTID_ != this_thread::get_id())
      {
         lock_ =
            make_unique<unique_lock<mutex>>(lockablePtr_->mu_, defer_lock);

         lock_->lock();
         lockablePtr_->mutexTID_ = this_thread::get_id();
         lockablePtr_->initAfterLock();
      }
   }
   
   ReentrantLock(ReentrantLock&& lock) :
      lockablePtr_(lock.lockablePtr_)
   {
      lock_ = move(lock.lock_);
   }



   ~ReentrantLock(void)
   {
      if (lock_ == nullptr)
         return;

      if (lock_->owns_lock())
      {
         if (lockablePtr_ != nullptr)
         {
            lockablePtr_->mutexTID_ = thread::id();
            lockablePtr_->cleanUpBeforeUnlock();
         }
      }
   }
};

#endif
