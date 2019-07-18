#include "CelerCreateOrderSequence.h"

#include "NettyCommunication.pb.h"
#include "UpstreamOrderProto.pb.h"

#include <QDate>

#include <spdlog/spdlog.h>

using namespace com::celertech::baseserver::communication::protobuf;
using namespace com::celertech::marketmerchant::api::order;
using namespace com::celertech::marketmerchant::api::enums::producttype;
using namespace com::celertech::marketmerchant::api::enums::accounttype;
using namespace com::celertech::marketmerchant::api::enums::ordertype;
using namespace com::celertech::marketmerchant::api::enums::handlinginstruction;
using namespace com::celertech::marketmerchant::api::enums::timeinforcetype;

CelerCreateOrderSequence::CelerCreateOrderSequence(const std::string& accountName
   , const QString &reqId, const bs::network::Quote& quote, const std::string &payoutTx
   , const std::shared_ptr<spdlog::logger>& logger)
 : CelerCommandSequence("CelerCreateOrderSequence", {
         { false, nullptr, &CelerCreateOrderSequence::createOrder }
   })
 , reqId_(reqId)
 , quote_(quote)
 , payoutTx_(payoutTx)
 , logger_(logger)
 , accountName_(accountName)
{}


bool CelerCreateOrderSequence::FinishSequence()
{
   return true;
}

CelerMessage CelerCreateOrderSequence::createOrder()
{
   CreateBitcoinOrderRequest request;

   auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());

   request.set_clientrequestid("bs.xbt.order." + std::to_string(timestamp.count()));
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

   request.set_producttype(bs::network::Asset::toCelerProductType(quote_.assetType));
   request.set_side(bs::network::Side::toCeler(bs::network::Side::invert(quote_.side)));

   request.set_handlinginstruction(AUTOMATED_NO_BROKER);
   request.set_timeinforce(FOK);

   if (!payoutTx_.empty()) {
      request.set_requestortransaction(payoutTx_);
   }

   if (quote_.assetType == bs::network::Asset::PrivateMarket) {
      request.set_requestorcointransactioninput(payoutTx_);
   }

   logger_->debug("[CreateBitcoinOrderRequest] {}", request.DebugString());

   CelerMessage message;
   message.messageType = CelerAPI::CreateBitcoinOrderRequestType;
   message.messageData = request.SerializeAsString();

   return message;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////

CelerFindAllOrdersSequence::CelerFindAllOrdersSequence(const std::shared_ptr<spdlog::logger> &logger)
   : CelerCommandSequence("CelerFindAllOrdersSequence", {
      { false, nullptr, &CelerFindAllOrdersSequence::create },
      { true, &CelerFindAllOrdersSequence::process, nullptr }
   })
   , logger_(logger)
{}

CelerMessage CelerFindAllOrdersSequence::create()
{
   FindAllOrderSnapshotsBySessionKey request;
   request.set_clientrequestid(GetSequenceId());

   logger_->debug("[FindAllOrderSnapshots] {}", request.DebugString());

   CelerMessage message;
   message.messageType = CelerAPI::FindAllOrdersType;
   message.messageData = request.SerializeAsString();

   return message;
}

bool CelerFindAllOrdersSequence::process(const CelerMessage &message)
{
   MultiResponseMessage response;
   if (!response.ParseFromString(message.messageData)) {
      logger_->error("[CelerFindAllOrdersSequence::process] failed to parse MultiResponseMessage");
      return false;
   }

   for (int i = 0; i < response.payload_size(); i++) {
      const ResponsePayload &payload = response.payload(i);
      const auto payloadType = CelerAPI::GetMessageType(payload.classname());
      messages_.push_back({payloadType, payload.contents()});
   }

   return true;
}

bool CelerFindAllOrdersSequence::FinishSequence()
{
   if (cbFinished_) {
      cbFinished_(messages_);
   }
   return true;
}
