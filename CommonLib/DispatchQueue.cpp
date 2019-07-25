#include "DispatchQueue.h"

DispatchQueue::DispatchQueue() = default;

DispatchQueue::~DispatchQueue() = default;

void DispatchQueue::dispatch(const Function& op)
{
   std::unique_lock<std::mutex> lock(lock_);
   q_.push(op);
   lock.unlock();
   cv_.notify_all();
}

void DispatchQueue::dispatch(Function&& op)
{
   std::unique_lock<std::mutex> lock(lock_);
   q_.push(std::move(op));
   lock.unlock();
   cv_.notify_all();
}

bool DispatchQueue::done() const
{
   std::unique_lock<std::mutex> lock(lock_);
   return quit_ && q_.empty();
}

void DispatchQueue::tryProcess()
{
   std::unique_lock<std::mutex> lock(lock_);

   cv_.wait(lock, [this] {
      return (!q_.empty() || quit_);
   });

   if (q_.empty()) {
      return;
   }

   auto op = std::move(q_.front());
   q_.pop();

   lock.unlock();

   op();
}

void DispatchQueue::quit()
{
   lock_.lock();
   quit_ = true;
   lock_.unlock();

   cv_.notify_all();
}
