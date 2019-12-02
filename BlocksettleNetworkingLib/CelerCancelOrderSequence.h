/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __CELER_CANCEL_ORDER_H__
#define __CELER_CANCEL_ORDER_H__

#include "CelerCommandSequence.h"
#include "CommonTypes.h"

#include <memory>
#include <string>
#include <functional>

namespace spdlog
{
   class logger;
}

class CelerCancelOrderSequence : public CelerCommandSequence<CelerCancelOrderSequence>
{
public:
   CelerCancelOrderSequence(int64_t orderId, const std::string& clientOrderId, const std::shared_ptr<spdlog::logger>& logger);
   ~CelerCancelOrderSequence() noexcept = default;

   CelerCancelOrderSequence(const CelerCancelOrderSequence&) = delete;
   CelerCancelOrderSequence& operator = (const CelerCancelOrderSequence&) = delete;

   CelerCancelOrderSequence(CelerCancelOrderSequence&&) = delete;
   CelerCancelOrderSequence& operator = (CelerCancelOrderSequence&&) = delete;

   bool FinishSequence() override;

private:
   CelerMessage cancelOrder();

   int64_t     orderId_;
   std::string clientOrderId_;
   std::shared_ptr<spdlog::logger> logger_;
};

#endif // __CELER_CANCEL_ORDER_H__
