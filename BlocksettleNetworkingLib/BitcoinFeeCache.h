/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __BITCOIN_FEE_CACHE_H__
#define __BITCOIN_FEE_CACHE_H__

#include <spdlog/spdlog.h>

#include <memory>
#include <mutex>
#include <chrono>
#include <unordered_map>
#include <vector>

class ArmoryConnection;

class BitcoinFeeCache
{
public:
   BitcoinFeeCache(const std::shared_ptr<spdlog::logger> &logger
                   , const std::shared_ptr<ArmoryConnection> &armory);
   ~BitcoinFeeCache() noexcept = default;

   BitcoinFeeCache(const BitcoinFeeCache&) = delete;
   BitcoinFeeCache& operator = (const BitcoinFeeCache&) = delete;

   BitcoinFeeCache(BitcoinFeeCache&&) = delete;
   BitcoinFeeCache& operator = (BitcoinFeeCache&&) = delete;

   // return s/b, not armory original bitcoin/kb
   using feeCB = std::function<void(float)>;

   bool getFeePerByteEstimation(unsigned int blocksToWait, const feeCB& cb);

private:
   bool cacheValueExpired(const std::chrono::system_clock::time_point& timestamp) const;

   void setFeeEstimationValue(const unsigned int blocksToWait, float fee);

private:
   std::shared_ptr<spdlog::logger>     logger_;
   std::shared_ptr<ArmoryConnection>   armory_;


   struct FeeEstimationCache
   {
      float                                  feeEstimation;
      std::chrono::system_clock::time_point  estimationTimestamp;
   };

   std::mutex                                            cacheMutex_;
   std::unordered_map<unsigned int, FeeEstimationCache>  estimationsCache_;
   std::unordered_map<unsigned int, std::vector<feeCB>>  pendingCB_;
};

#endif
