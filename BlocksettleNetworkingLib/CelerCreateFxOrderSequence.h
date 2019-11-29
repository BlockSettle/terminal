/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __CELER_CREATE_FX_ORDER_H__
#define __CELER_CREATE_FX_ORDER_H__

#include "CelerCommandSequence.h"
#include "CommonTypes.h"
#include <memory>
#include <string>


namespace spdlog {
   class logger;
}

class CelerCreateFxOrderSequence : public CelerCommandSequence<CelerCreateFxOrderSequence>
{
public:
   CelerCreateFxOrderSequence(const std::string& accountName, const QString &reqId, const bs::network::Quote& quote, const std::shared_ptr<spdlog::logger>& logger);
   ~CelerCreateFxOrderSequence() noexcept = default;

   CelerCreateFxOrderSequence(const CelerCreateFxOrderSequence&) = delete;
   CelerCreateFxOrderSequence& operator = (const CelerCreateFxOrderSequence&) = delete;
   CelerCreateFxOrderSequence(CelerCreateFxOrderSequence&&) = delete;
   CelerCreateFxOrderSequence& operator = (CelerCreateFxOrderSequence&&) = delete;

   bool FinishSequence() override;

private:
   CelerMessage createOrder();

   QString              reqId_;
   bs::network::Quote   quote_;
   std::shared_ptr<spdlog::logger> logger_;
   const std::string accountName_;
};

#endif // __CELER_CREATE_FX_ORDER_H__
