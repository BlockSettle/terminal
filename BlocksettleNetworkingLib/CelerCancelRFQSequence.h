/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __CELER_CANCEL_RFQ_SEQUENCE_H__
#define __CELER_CANCEL_RFQ_SEQUENCE_H__

#include "CelerCommandSequence.h"
#include "CommonTypes.h"
#include <string>
#include <functional>
#include <memory>

namespace spdlog {
   class logger;
}

namespace bs {
   namespace network {

      class CelerCancelRFQSequence : public CelerCommandSequence<CelerCancelRFQSequence>
      {
      public:
         CelerCancelRFQSequence(const QString &reqId, const std::shared_ptr<spdlog::logger>& logger);
         ~CelerCancelRFQSequence() noexcept override = default;

         CelerCancelRFQSequence(const CelerCancelRFQSequence&) = delete;
         CelerCancelRFQSequence& operator = (const CelerCancelRFQSequence&) = delete;
         CelerCancelRFQSequence(CelerCancelRFQSequence&&) = delete;
         CelerCancelRFQSequence& operator = (CelerCancelRFQSequence&&) = delete;

         bool FinishSequence() override;

      private:
         CelerMessage cancelRFQ();

         QString  reqId_;
         std::shared_ptr<spdlog::logger> logger_;
      };

   }  //namespace network
}  //namespace bs

#endif // __CELER_CANCEL_RFQ_SEQUENCE_H__
