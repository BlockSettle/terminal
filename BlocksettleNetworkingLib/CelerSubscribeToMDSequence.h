/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __CELER_SUBSCRIBE_TO_MD_H__
#define __CELER_SUBSCRIBE_TO_MD_H__

#include "CelerCommandSequence.h"

#include "CommonTypes.h"

#include <string>
#include <functional>
#include <memory>

namespace spdlog
{
   class logger;
}

class CelerSubscribeToMDSequence : public CelerCommandSequence<CelerSubscribeToMDSequence>
{
public:
   CelerSubscribeToMDSequence(const std::string& currencyPair, bs::network::Asset::Type at, const std::shared_ptr<spdlog::logger>& logger);
   ~CelerSubscribeToMDSequence() noexcept override = default;

   CelerSubscribeToMDSequence(const CelerSubscribeToMDSequence&) = delete;
   CelerSubscribeToMDSequence& operator = (const CelerSubscribeToMDSequence&) = delete;

   CelerSubscribeToMDSequence(CelerSubscribeToMDSequence&&) = delete;
   CelerSubscribeToMDSequence& operator = (CelerSubscribeToMDSequence&&) = delete;

   bool FinishSequence() override;
   const std::string getReqId() const { return reqId_; }

private:
   CelerMessage subscribeToMD();
   CelerMessage subscribeToMDStatistics();

   std::string currencyPair_;
   bs::network::Asset::Type assetType_;
   std::string reqId_;
   std::shared_ptr<spdlog::logger> logger_;
};

#endif // __CELER_SUBSCRIBE_TO_MD_H__
