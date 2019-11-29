/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "CelerCancelOrderSequence.h"

#include "NettyCommunication.pb.h"
#include "UpstreamOrderProto.pb.h"

#include <spdlog/spdlog.h>

using namespace com::celertech::baseserver::communication::protobuf;
using namespace com::celertech::marketmerchant::api::order;
using namespace com::celertech::marketmerchant::api::enums::producttype;
using namespace com::celertech::marketmerchant::api::enums::accounttype;
using namespace com::celertech::marketmerchant::api::enums::ordertype;
using namespace com::celertech::marketmerchant::api::enums::handlinginstruction;
using namespace com::celertech::marketmerchant::api::enums::timeinforcetype;

CelerCancelOrderSequence::CelerCancelOrderSequence(int64_t orderId
   , const std::string& clientOrderId
   , const std::shared_ptr<spdlog::logger>& logger)
 : CelerCommandSequence("CelerCancelOrderSequence", {
         { false, nullptr, &CelerCancelOrderSequence::cancelOrder }
   })
 , orderId_(orderId)
 , clientOrderId_(clientOrderId)
 , logger_(logger)
{}


bool CelerCancelOrderSequence::FinishSequence()
{
   return true;
}

CelerMessage CelerCancelOrderSequence::cancelOrder()
{
   CancelOrderRequest request;

   request.set_orderid(orderId_);
   request.set_clordid(clientOrderId_);

   logger_->debug("[CelerCancelOrderSequence] {}", request.DebugString());

   CelerMessage message;
   message.messageType = CelerAPI::CreateBitcoinOrderRequestType;
   message.messageData = request.SerializeAsString();

   return message;
}
