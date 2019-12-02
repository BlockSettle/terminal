/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __CELER_CANCEL_QUOTE_NOTIFICATION_SEQUENCE_H__
#define __CELER_CANCEL_QUOTE_NOTIFICATION_SEQUENCE_H__

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

      class CelerCancelQuoteNotifSequence : public CelerCommandSequence<CelerCancelQuoteNotifSequence>
      {
      public:
         CelerCancelQuoteNotifSequence(const QString &reqId, const QString &reqSessToken, const std::shared_ptr<spdlog::logger>& logger);
         ~CelerCancelQuoteNotifSequence() noexcept = default;

         CelerCancelQuoteNotifSequence(const CelerCancelQuoteNotifSequence&) = delete;
         CelerCancelQuoteNotifSequence& operator = (const CelerCancelQuoteNotifSequence&) = delete;
         CelerCancelQuoteNotifSequence(CelerCancelQuoteNotifSequence&&) = delete;
         CelerCancelQuoteNotifSequence& operator = (CelerCancelQuoteNotifSequence&&) = delete;

         bool FinishSequence() override;

      private:
         CelerMessage send();

         QString  reqId_, reqSessToken_;
         std::shared_ptr<spdlog::logger> logger_;
      };

   }  //namespace network
}  //namespace bs

#endif // __CELER_CANCEL_QUOTE_NOTIFICATION_SEQUENCE_H__
