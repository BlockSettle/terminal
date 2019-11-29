/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "CelerCreateFxOrderSequence.h"
#include "UpstreamOrderProto.pb.h"
#include <QDate>
#include <spdlog/spdlog.h>

using namespace com::celertech::marketmerchant::api::order;
using namespace com::celertech::marketmerchant::api::enums::accounttype;
using namespace com::celertech::marketmerchant::api::enums::ordertype;
using namespace com::celertech::marketmerchant::api::enums::handlinginstruction;
using namespace com::celertech::marketmerchant::api::enums::timeinforcetype;

CelerCreateFxOrderSequence::CelerCreateFxOrderSequence(const std::string& accountName
   , const QString &reqId, const bs::network::Quote& quote
   , const std::shared_ptr<spdlog::logger>& logger)
 : CelerCommandSequence("CelerCreateFxOrderSequence",
      {
         { false, nullptr, &CelerCreateFxOrderSequence::createOrder }
      })
 , reqId_(reqId)
 , quote_(quote)
 , logger_(logger)
 , accountName_(accountName)
{}


bool CelerCreateFxOrderSequence::FinishSequence()
{
   return true;
}

CelerMessage CelerCreateFxOrderSequence::createOrder()
{
   CreateFxOrderRequest request;

   auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());

   request.set_clientrequestid("bs.fx.order." + std::to_string(timestamp.count()));
   request.set_clorderid(reqId_.toStdString());
   request.set_quoteid(quote_.quoteId);
   request.set_accounttype(CLIENT);
   request.set_account(accountName_);
   request.set_ordertype(PREVIOUSLY_QUOTED);

   request.set_securitycode(quote_.security);
   request.set_securityid(quote_.security);
   request.set_price(quote_.price);
   request.set_qty(quote_.quantity);
   request.set_currency(quote_.product);

   request.set_producttype(com::celertech::marketmerchant::api::enums::producttype::ProductType::SPOT);
   request.set_side(bs::network::Side::toCeler(bs::network::Side::invert(quote_.side)));

   request.set_handlinginstruction(AUTOMATED_NO_BROKER);
   request.set_timeinforce(FOK);
   request.set_settlementdate(QDate::currentDate().toString(Qt::ISODate).toStdString());

   CreateFxStrategyLegOrder *leg = request.add_leg();
   leg->set_underlyingcode(quote_.security);
   leg->set_underlyingsecurityid(quote_.security);
   leg->set_legvaluedate(QDate::currentDate().toString(Qt::ISODate).toStdString());
   leg->set_side(request.side());
   leg->set_qty(quote_.quantity);
   leg->set_price(quote_.price);
   leg->set_settlementtype("SP");
   
   CelerMessage message;
   message.messageType = CelerAPI::CreateFxOrderRequestType;
   message.messageData = request.SerializeAsString();

   logger_->debug("{}", request.DebugString());

   return message;
}
