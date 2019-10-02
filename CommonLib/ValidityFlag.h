#ifndef VALIDITY_FLAG_H
#define VALIDITY_FLAG_H

// Use to detect in when some class was destroyed (implementation is thread-safe).
// Destructor would be blocked while handle(s) locked.
//
// Example (single thread usage, no need to call lock/unlock):
//   class Test
//   {
//      Test()
//      {
//         auto safeCallback = [this, handle = validityFlag_.handle()] {
//            if (!handle.isValid()) {
//               return;
//            }
//            // proceed..
//         };
//      }
//   private:
//      ValidityFlag validityFlag_;
//   };
//
// Example (multithread usage):
//   class Test
//   {
//      Test()
//      {
//         auto safeCallback = [this, handle = validityFlag_.handle()] () mutable {
//         std::lock_guard<ValidityHandle> lock(handle);
//            if (!handle.isValid()) {
//               return;
//            }
//            // proceed..
//         };
//      }
//   private:
//      ValidityFlag validityFlag_;
//   };

#include <memory>
#include <mutex>

struct ValidityFlagData;

class ValidityHandle
{
public:
   ValidityHandle(const std::shared_ptr<ValidityFlagData> &flag);
   ~ValidityHandle();

   ValidityHandle(const ValidityHandle &other);
   ValidityHandle& operator = (const ValidityHandle &other);

   ValidityHandle(ValidityHandle &&other);
   ValidityHandle& operator = (ValidityHandle &&other);

   // Blocks parent object destructor if needed.
   void lock();

   // Releases parent object.
   void unlock();

   // Checks if parent object is still valid.
   bool isValid() const;

private:
   std::shared_ptr<ValidityFlagData> data_;

};

using ValidityGuard = std::lock_guard<ValidityHandle>;

class ValidityFlag
{
public:
   ValidityFlag();
   ~ValidityFlag();

   ValidityFlag(const ValidityFlag&) = delete;
   ValidityFlag &operator = (const ValidityFlag&) = delete;

   ValidityFlag(ValidityFlag &&other);
   ValidityFlag &operator=(ValidityFlag &&other);

   // Creates new handle that points to this object.
   // Method is not thread-safe.
   ValidityHandle handle() const;

   // Marks as invalid. Creating new handles is not possible after that.
   // Method is not thread-safe.
   void reset();

private:
   std::shared_ptr<ValidityFlagData> data_;

};

#endif
