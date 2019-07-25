#ifndef DISPATCH_QUEUE_H
#define DISPATCH_QUEUE_H

#include <functional>
#include <queue>
#include <mutex>
#include <condition_variable>

// Simple multiple producers/single consumer dispatcher queue.
// Could be used to run functions on different thread.

class DispatchQueue {
public:
   // Function type
   using Function = std::function<void(void)>;

   DispatchQueue();
   ~DispatchQueue();

   DispatchQueue(const DispatchQueue&) = delete;
   DispatchQueue& operator=(const DispatchQueue&) = delete;
   DispatchQueue(DispatchQueue&&) = delete;
   DispatchQueue& operator=(DispatchQueue&&) = delete;

   // Run function on different thread. Thread-safe.
   void dispatch(const Function& op);

   // Run function on different thread. Thread-safe.
   void dispatch(Function&& op);

   // Returns true if quit was requested and queue is empty.
   bool done() const;

   // Try process single function.
   void tryProcess();

   // Sets quit flag. Thread-safe.
   void quit();
private:
   mutable std::mutex lock_;

   std::queue<Function> q_;

   std::condition_variable cv_;

   bool quit_{false};
};

#endif // DISPATCH_QUEUE_H
