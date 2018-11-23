#include "CelerSubscribeToMDSequence.h"

#include <QDate>

#include <spdlog/spdlog.h>

#include "com/celertech/marketdata/api/marketstatistic/UpstreamMarketStatisticProto.pb.h"

CelerSubscribeToMDSequence::CelerSubscribeToMDSequence(const std::string& currencyPair, bs::network::Asset::Type at, const std::shared_ptr<spdlog::logger>& logger)
 : CelerCommandSequence("CelerSubscribeToMDSequence",
      {
         { false, nullptr, &CelerSubscribeToMDSequence::subscribeToMD}
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
   com::celertech::marketdata::api::marketstatistic::MarketStatisticRequest request;

   reqId_ = GetUniqueId();

   request.set_marketstatisticrequestid(reqId_);
   request.set_marketdatarequesttype(com::celertech::marketdata::api::enums::marketdatarequesttype::SNAPSHOT_PLUS_UPDATES);
   request.set_securitycode(currencyPair_);
   request.set_securityid(currencyPair_);
   request.set_producttype(bs::network::Asset::toCelerMDProductType(assetType_));
   request.set_assettype(bs::network::Asset::toCelerMDAssetType(assetType_));

   logger_->debug("[CelerSubscribeToMDSequence::subscribeToMD] subscribe request:\n{}"
      , request.DebugString());

   CelerMessage message;
   message.messageType = CelerAPI::MarketStatisticRequestType;
   message.messageData = request.SerializeAsString();

   return message;
}