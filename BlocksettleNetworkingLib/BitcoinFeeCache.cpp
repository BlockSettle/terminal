/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "BitcoinFeeCache.h"

#include "ArmoryConnection.h"

#include <QtGlobal>

static constexpr auto kCacheValueExpireTimeout = std::chrono::hours(1);
// 200 s/b
static constexpr float kFallbackFeeAmount = 0.000002;


BitcoinFeeCache::BitcoinFeeCache(const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<ArmoryConnection> &armory)
 : logger_{logger}
 , armory_{armory}
{
}

//fee = ArmoryConnection::toFeePerByte(fee);
bool BitcoinFeeCache::getFeePerByteEstimation(unsigned int blocksToWait, const feeCB& cb)
{
   if (blocksToWait < 2) {
      blocksToWait = 2;
   } else if (blocksToWait > 144) {
      blocksToWait = 144;
   }

   bool  estimateScheduled = false;
   float result = 0;

   {
      std::lock_guard<std::mutex> lock(cacheMutex_);

      auto it = estimationsCache_.find(blocksToWait);
      if ((it == estimationsCache_.end()) || cacheValueExpired(it->second.estimationTimestamp)) {
         auto it = pendingCB_.find(blocksToWait);

         estimateScheduled = true;

         if (it == pendingCB_.end()) {
            pendingCB_.emplace(blocksToWait, std::vector<feeCB>{ cb });

            auto cbWrap = [this, blocksToWait](float fee) {
               setFeeEstimationValue(blocksToWait, ArmoryConnection::toFeePerByte(fee));
            };

            if (!armory_->estimateFee(blocksToWait, cbWrap)) {
               pendingCB_.erase(blocksToWait);
               return false;
            }

         } else {
            it->second.emplace_back(cb);
         }
      } else {
         estimateScheduled = false;
         result = it->second.feeEstimation;
      }
   }

   if (!estimateScheduled) {
      cb(result);
   }

   return true;
}

void BitcoinFeeCache::setFeeEstimationValue(const unsigned int blocksToWait, float fee)
{
   std::vector<feeCB> userCB;

   {
      std::lock_guard<std::mutex> lock(cacheMutex_);

      if (qFuzzyIsNull(fee)) {
         fee = kFallbackFeeAmount;
      } else {
         FeeEstimationCache currentValue;
         currentValue.feeEstimation = fee;
         currentValue.estimationTimestamp = std::chrono::system_clock::now();

         estimationsCache_[blocksToWait] = currentValue;
      }

      auto cbIt = pendingCB_.find(blocksToWait);
      if (cbIt != pendingCB_.end()) {
         userCB = std::move(cbIt->second);

         pendingCB_.erase(cbIt);
      }
   }

   if (userCB.empty()) {
      logger_->error("[BitcoinFeeCache::setFeeEstimationValue] no CB saved for {} blocks"
         , blocksToWait);
   }

   for (const auto& cb : userCB) {
      cb(fee);
   }
}

bool BitcoinFeeCache::cacheValueExpired(const std::chrono::system_clock::time_point& timestamp) const
{
   return (std::chrono::system_clock::now() - timestamp) > kCacheValueExpireTimeout;
}
