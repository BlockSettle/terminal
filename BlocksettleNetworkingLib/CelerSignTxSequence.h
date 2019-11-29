/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __CELER_SIGN_TX_H__
#define __CELER_SIGN_TX_H__

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

      class CelerSignTxSequence : public CelerCommandSequence<CelerSignTxSequence>
      {
      public:
         CelerSignTxSequence(const QString &orderId, const std::string &txData, const std::shared_ptr<spdlog::logger>& logger);
         ~CelerSignTxSequence() noexcept = default;

         CelerSignTxSequence(const CelerSignTxSequence&) = delete;
         CelerSignTxSequence& operator = (const CelerSignTxSequence&) = delete;
         CelerSignTxSequence(CelerSignTxSequence&&) = delete;
         CelerSignTxSequence& operator = (CelerSignTxSequence&&) = delete;

         bool FinishSequence() override { return true; }

      private:
         CelerMessage send();

         QString           orderId_;
         const std::string txData_;
         std::shared_ptr<spdlog::logger> logger_;
      };

   }  //namespace network
}  //namespace bs

#endif // __CELER_SIGN_TX_H__
