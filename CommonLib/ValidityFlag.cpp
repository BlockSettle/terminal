#include "ValidityFlag.h"

#include <cassert>
#include <mutex>

struct ValidityFlagData
{
   std::recursive_mutex mutex;
   bool isValid{true};
};

ValidityHandle::ValidityHandle() = default;

ValidityHandle::ValidityHandle(const std::shared_ptr<ValidityFlagData> &data)
   : data_(data)
{
}

ValidityHandle &ValidityHandle::operator=(const ValidityHandle &other)
{
   data_ = other.data_;
   return *this;
}

ValidityHandle::~ValidityHandle() = default;

ValidityHandle::ValidityHandle(const ValidityHandle &other) = default;

ValidityHandle::ValidityHandle(ValidityHandle &&other)
   : data_(std::move(other.data_))
{
}

ValidityHandle &ValidityHandle::operator=(ValidityHandle &&other)
{
   data_ = std::move(other.data_);
   return *this;
}

void ValidityHandle::lock()
{
   data_->mutex.lock();
}

void ValidityHandle::unlock()
{
   data_->mutex.unlock();
}

bool ValidityHandle::isValid() const
{
   return data_ && data_->isValid;
}

ValidityFlag::ValidityFlag()
   : data_(std::make_shared<ValidityFlagData>())
{
}

ValidityFlag::~ValidityFlag()
{
   reset();
}

ValidityFlag::ValidityFlag(ValidityFlag &&other)
   : data_(std::move(other.data_))
{
}

ValidityFlag &ValidityFlag::operator=(ValidityFlag &&other)
{
   data_ = std::move(other.data_);
   return *this;
}

ValidityHandle ValidityFlag::handle() const
{
   assert(data_);
   return ValidityHandle(data_);
}

void ValidityFlag::reset()
{
   if (data_) {
      std::lock_guard<std::recursive_mutex> lock(data_->mutex);
      data_->isValid = false;
   }
}
