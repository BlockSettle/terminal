#include "AddressVerificationPool.h"

#include "AddressVerificator.h"
#include "FastLock.h"

#include <spdlog/spdlog.h>

#include <QString>

AddressVerificationPool::AddressVerificationPool(const std::shared_ptr<spdlog::logger>& logger, const std::string& poolId)
   : logger_(logger)
   , poolId_(poolId)
{
   verificator_ = std::make_shared<AddressVerificator>(logger_, poolId_
   , [this](const std::shared_ptr<AuthAddress>& address, AddressVerificationState state)
      {
         completeVerification(address, state);
      });
}

bool AddressVerificationPool::SubmitForVerification(const std::shared_ptr<AuthAddress>& address
      , const verificationCompletedCallback& onCompleted)
{
   auto addressString = address->GetChainedAddress().display<std::string>();

   logger_->debug("[AddressVerificationPool::SubmitForVerification] {}: submitting {}"
      , poolId_, addressString);

   {
      FastLock locker(pendingLockerFlag_);
      pendingResults_.emplace(addressString, onCompleted);
   }

   verificator_->StartAddressVerification(address);
   verificator_->RegisterAddresses();

   return true;
}

void AddressVerificationPool::completeVerification(const std::shared_ptr<AuthAddress>& address, AddressVerificationState state)
{
   auto addressString = address->GetChainedAddress().display<std::string>();
   verificationCompletedCallback onCompleted;

   size_t pendingCount = 0;
   {
      FastLock locker(pendingLockerFlag_);
      auto it = pendingResults_.find(addressString);
      if (it != pendingResults_.end()) {
         onCompleted = std::move(it->second);
         pendingResults_.erase(it);
      }
      pendingCount = pendingResults_.size();
   }

   if (!onCompleted) {
      logger_->error("[AddressVerificationPool::completeVerification] {}: no pending verifications for {}. Pending count {}"
         , poolId_, addressString, pendingCount);
      return;
   }

   logger_->debug("[AddressVerificationPool::completeVerification] {} : {} is {}"
      , poolId_, addressString, to_string(state));

   onCompleted(state);
}

bool AddressVerificationPool::SetBSAddressList(const std::unordered_set<std::string>& addressList)
{
   const bool rc = verificator_->SetBSAddressList(addressList);
   verificator_->RegisterBSAuthAddresses();
   return rc;
}
