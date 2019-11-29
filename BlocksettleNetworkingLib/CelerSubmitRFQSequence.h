/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __CELER_SUBMIT_RFQ_H__
#define __CELER_SUBMIT_RFQ_H__

#include "CelerCommandSequence.h"
#include "CommonTypes.h"
#include <string>
#include <functional>
#include <memory>

namespace spdlog
{
   class logger;
}


class CelerSubmitRFQSequence : public CelerCommandSequence<CelerSubmitRFQSequence>
{
public:
   CelerSubmitRFQSequence(const std::string& accountName
      , const bs::network::RFQ& rfq
      , const std::shared_ptr<spdlog::logger>& logger
      , bool debugPrintRFQ = true);
   ~CelerSubmitRFQSequence() noexcept = default;

   CelerSubmitRFQSequence(const CelerSubmitRFQSequence&) = delete;
   CelerSubmitRFQSequence& operator = (const CelerSubmitRFQSequence&) = delete;

   CelerSubmitRFQSequence(CelerSubmitRFQSequence&&) = delete;
   CelerSubmitRFQSequence& operator = (CelerSubmitRFQSequence&&) = delete;

   bool FinishSequence() override;

private:
   CelerMessage submitRFQ();

   const std::string accountName_;
   bs::network::RFQ rfq_;
   std::shared_ptr<spdlog::logger> logger_;

   bool debugPrintRFQ_;
};

#endif // __CELER_SUBMIT_RFQ_H__
