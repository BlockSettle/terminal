/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __CELER_SUBMIT_QUOTE_NOTIF_H__
#define __CELER_SUBMIT_QUOTE_NOTIF_H__

#include "CelerCommandSequence.h"
#include "CommonTypes.h"
#include <string>
#include <functional>
#include <memory>

namespace spdlog
{
   class logger;
}

namespace bs {
   namespace network {

      class CelerSubmitQuoteNotifSequence : public CelerCommandSequence<CelerSubmitQuoteNotifSequence>
      {
      public:
         CelerSubmitQuoteNotifSequence(const std::string& accountName, const QuoteNotification &qn, const std::shared_ptr<spdlog::logger>& logger);
         ~CelerSubmitQuoteNotifSequence() noexcept = default;

         CelerSubmitQuoteNotifSequence(const CelerSubmitQuoteNotifSequence&) = delete;
         CelerSubmitQuoteNotifSequence& operator = (const CelerSubmitQuoteNotifSequence&) = delete;
         CelerSubmitQuoteNotifSequence(CelerSubmitQuoteNotifSequence&&) = delete;
         CelerSubmitQuoteNotifSequence& operator = (CelerSubmitQuoteNotifSequence&&) = delete;

         bool FinishSequence() override;

      private:
         CelerMessage submitQuoteNotif();

         const std::string accountName_;
         QuoteNotification qn_;
         std::shared_ptr<spdlog::logger> logger_;
      };
   }  //namespace network
}  //namespace bs


#endif // __CELER_SUBMIT_RFQ_H__
