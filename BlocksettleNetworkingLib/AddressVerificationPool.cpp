/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
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
   verificator_ = std::make_shared<AddressVerificator>(logger_, armory
   , [this](const bs::Address &address, AddressVerificationState state)
      {
         completeVerification(address, state);
      });
}

bool AddressVerificationPool::submitForVerification(const bs::Address &address
      , const verificationCompletedCallback& onCompleted)
{
   const auto addressStr = address.display();
   unsigned int pendingVerifications = 0;

   {
      FastLock locker(pendingLockerFlag_);
      auto it = pendingResults_.find(addressStr);
      if (it == pendingResults_.end()) {
         std::queue<verificationCompletedCallback> q;
         q.emplace(onCompleted);
         pendingResults_.emplace(addressStr, std::move(q));
         pendingVerifications = 1;
      } else {
         it->second.emplace(onCompleted);
         pendingVerifications = it->second.size();
      }
   }

   logger_->debug("[AddressVerificationPool::SubmitForVerification] {}: submitting {}. Pending verification for address: {}"
      , poolId_, addressStr, pendingVerifications);

   verificator_->addAddress(address);
   verificator_->startAddressVerification();

   return true;
}

void AddressVerificationPool::completeVerification(const bs::Address &address, AddressVerificationState state)
{
   auto addressString = address.display();
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
   return rc;
}
