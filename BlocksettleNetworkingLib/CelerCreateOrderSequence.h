/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __CELER_CREATE_ORDER_H__
#define __CELER_CREATE_ORDER_H__

#include "CelerCommandSequence.h"
#include "CommonTypes.h"
#include <memory>
#include <string>
#include <functional>

namespace spdlog {
   class logger;
}

class CelerCreateOrderSequence : public CelerCommandSequence<CelerCreateOrderSequence>
{
public:
   CelerCreateOrderSequence(const std::string& accountName, const QString &reqId, const bs::network::Quote& quote, const std::string &payoutTx
      , const std::shared_ptr<spdlog::logger>& logger);

   CelerCreateOrderSequence(const CelerCreateOrderSequence&) = delete;
   CelerCreateOrderSequence& operator = (const CelerCreateOrderSequence&) = delete;

   CelerCreateOrderSequence(CelerCreateOrderSequence&&) = delete;
   CelerCreateOrderSequence& operator = (CelerCreateOrderSequence&&) = delete;

   bool FinishSequence() override;

private:
   CelerMessage createOrder();

   QString              reqId_;
   bs::network::Quote   quote_;
   std::string          payoutTx_;
   std::shared_ptr<spdlog::logger> logger_;
   const std::string accountName_;
};

#endif // __CELER_CREATE_ORDER_H__
