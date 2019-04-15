#include "AddressVerificationPool.h"

#include "AddressVerificator.h"
#include "FastLock.h"

#include <spdlog/spdlog.h>


AddressVerificationPool::AddressVerificationPool(const std::shared_ptr<spdlog::logger>& logger
   , const std::string& poolId
   , const std::shared_ptr<ArmoryConnection> &armory)
   : logger_(logger)
   , poolId_(poolId)
{
   verificator_ = std::make_shared<AddressVerificator>(logger_, armory, poolId_
   , [this](const std::shared_ptr<AuthAddress>& address, AddressVerificationState state)
      {
         completeVerification(address, state);
      });
}

bool AddressVerificationPool::SubmitForVerification(const std::shared_ptr<AuthAddress>& address
      , const verificationCompletedCallback& onCompleted)
{
   auto addressString = address->GetChainedAddress().display();

   unsigned int pendingVerifications = 0;

   {
      FastLock locker(pendingLockerFlag_);
      auto it = pendingResults_.find(addressString);
      if (it == pendingResults_.end()) {
         std::queue<verificationCompletedCallback> q;
         q.emplace(onCompleted);
         pendingResults_.emplace(addressString, std::move(q));
         pendingVerifications = 1;
      } else {
         it->second.emplace(onCompleted);
         pendingVerifications = it->second.size();
      }
   }

   logger_->debug("[AddressVerificationPool::SubmitForVerification] {}: submitting {}. Pending verification for address: {}"
      , poolId_, addressString, pendingVerifications);

   verificator_->StartAddressVerification(address);
   verificator_->RegisterAddresses();

   return true;
}

void AddressVerificationPool::completeVerification(const std::shared_ptr<AuthAddress>& address, AddressVerificationState state)
{
   auto addressString = address->GetChainedAddress().display();
   std::queue<verificationCompletedCallback> callbackQueue;

   size_t pendingCount = 0;
   {
      FastLock locker(pendingLockerFlag_);
      auto it = pendingResults_.find(addressString);
      if (it != pendingResults_.end()) {
         callbackQueue = std::move(it->second);
         pendingResults_.erase(it);
      }
      pendingCount = pendingResults_.size();
   }

   if (callbackQueue.empty()) {
      logger_->error("[AddressVerificationPool::completeVerification] {}: no pending verifications for {}. Pending count {}"
         , poolId_, addressString, pendingCount);
      return;
   }

   logger_->debug("[AddressVerificationPool::completeVerification] {} : {} is {}"
      , poolId_, addressString, to_string(state));

   while (!callbackQueue.empty()) {
      auto onCompleted = callbackQueue.front();
      callbackQueue.pop();
      onCompleted(state);
   }
}

bool AddressVerificationPool::SetBSAddressList(const std::unordered_set<std::string>& addressList)
{
   const bool rc = verificator_->SetBSAddressList(addressList);
   verificator_->RegisterBSAuthAddresses();
   return rc;
}
