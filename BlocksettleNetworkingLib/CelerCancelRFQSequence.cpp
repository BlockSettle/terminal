/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "CelerCancelRFQSequence.h"

#include "UpstreamQuoteProto.pb.h"

#include <QDate>

#include <spdlog/spdlog.h>

using namespace bs::network;
using namespace com::celertech::marketmerchant::api::quote;

CelerCancelRFQSequence::CelerCancelRFQSequence(const QString &reqId, const std::shared_ptr<spdlog::logger>& logger)
 : CelerCommandSequence("CelerCancelRFQSequence", {
         { false, nullptr, &CelerCancelRFQSequence::cancelRFQ }
      })
   , reqId_(reqId)
   , logger_(logger)
{}


bool CelerCancelRFQSequence::FinishSequence()
{
   return true;
}

CelerMessage CelerCancelRFQSequence::cancelRFQ()
{
   QuoteCancelRequest request;

   request.set_quoterequestid(reqId_.toStdString());
   request.set_quotecanceltype(com::celertech::marketmerchant::api::enums::quotecanceltype::CANCEL_QUOTE_SPECIFIED_IN_QUOTEID);

   CelerMessage message;
   message.messageType = CelerAPI::QuoteCancelRequestType;
   message.messageData = request.SerializeAsString();

   logger_->debug("CancelRFQ: {}", request.DebugString());

   return message;
}
