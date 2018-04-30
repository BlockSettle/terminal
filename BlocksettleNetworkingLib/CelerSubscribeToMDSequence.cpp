#include "CelerSubscribeToMDSequence.h"

#include "UpstreamMarketDataProto.pb.h"
#include "UpstreamMarketStatisticProto.pb.h"
#include "MarketDataRequestTypeProto.pb.h"
#include "MarketDataUpdateTypeProto.pb.h"

#include <QDate>

#include <spdlog/spdlog.h>

using namespace com::celertech::marketmerchant::api::marketdata;
using namespace com::celertech::marketmerchant::api::marketstatistic;
using namespace com::celertech::marketmerchant::api::enums::marketdatarequesttype;
using namespace com::celertech::marketmerchant::api::enums::marketdataupdatetype;

CelerSubscribeToMDSequence::CelerSubscribeToMDSequence(const std::string& currencyPair, bs::network::Asset::Type at, const std::shared_ptr<spdlog::logger>& logger)
 : CelerCommandSequence("CelerSubscribeToMDSequence",
      {
         { false, nullptr, &CelerSubscribeToMDSequence::subscribeToMD},
         { false, nullptr, &CelerSubscribeToMDSequence::subscribeToMDStat}
      })
   , currencyPair_(currencyPair)
   , assetType_(at)
   , logger_(logger)
{}

bool CelerSubscribeToMDSequence::FinishSequence()
{
   return true;
}

CelerMessage CelerSubscribeToMDSequence::subscribeToMD()
{
   MarketDataRequest request;
   reqId_ = GetUniqueId();

   request.set_marketdatarequestid(reqId_);
   request.set_marketdatarequesttype(SNAPSHOT_PLUS_UPDATES);
   request.set_marketdataupdatetype(FULL_SNAPSHOT);
   request.set_marketdepth(0);
   request.set_securitycode(currencyPair_);
   request.set_securityid(currencyPair_);
   request.set_streamid("BLK_STANDARD");
   request.set_assettype(bs::network::Asset::toCeler(assetType_));
   request.set_producttype(bs::network::Asset::toCelerProductType(assetType_));
//   logger_->debug("MD req: {}", request.DebugString());

   if (assetType_ == bs::network::Asset::SpotFX) {
      request.set_settlementtype("SP");
   }

   CelerMessage message;
   message.messageType = CelerAPI::MarketDataRequestType;
   message.messageData = request.SerializeAsString();

   return message;
}

CelerMessage CelerSubscribeToMDSequence::subscribeToMDStat()
{
   MarketStatisticRequest request;
   reqId_ = GetUniqueId();

   request.set_marketstatisticrequestid(reqId_);
   request.set_marketdatarequesttype(SNAPSHOT_PLUS_UPDATES);
   request.set_securitycode(currencyPair_);
   request.set_securityid(currencyPair_);
   request.set_assettype(bs::network::Asset::toCeler(assetType_));
   request.set_producttype(bs::network::Asset::toCelerProductType(assetType_));
//   logger_->debug("MDStats req: {}", request.DebugString());

   CelerMessage message;
   message.messageType = CelerAPI::MarketStatsRequestType;
   message.messageData = request.SerializeAsString();

   return message;
}
