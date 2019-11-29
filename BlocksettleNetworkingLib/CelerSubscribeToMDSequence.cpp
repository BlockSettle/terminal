/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "CelerSubscribeToMDSequence.h"

#include <QDate>

#include <spdlog/spdlog.h>

#include "com/celertech/marketdata/api/marketstatistic/UpstreamMarketStatisticProto.pb.h"
#include "com/celertech/marketdata/api/price/UpstreamPriceProto.pb.h"

CelerSubscribeToMDSequence::CelerSubscribeToMDSequence(const std::string& currencyPair, bs::network::Asset::Type at, const std::shared_ptr<spdlog::logger>& logger)
 : CelerCommandSequence("CelerSubscribeToMDSequence",
      {
         { false, nullptr, &CelerSubscribeToMDSequence::subscribeToMD},
         { false, nullptr, &CelerSubscribeToMDSequence::subscribeToMDStatistics}
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
   com::celertech::marketdata::api::price::MarketDataSubscriptionRequest request;

   reqId_ = GetUniqueId();

   request.set_marketdatarequestid(reqId_);
   request.set_marketdatarequesttype(com::celertech::marketdata::api::enums::marketdatarequesttype::SNAPSHOT_PLUS_UPDATES);
   request.set_marketdataupdatetype(com::celertech::marketdata::api::enums::marketdataupdatetype::FULL_SNAPSHOT);
   request.set_securitycode(currencyPair_);
   request.set_securityid(currencyPair_);
   request.set_marketdatabooktype(com::celertech::marketdata::api::enums::marketdatabooktype::FULL_BOOK);
   request.set_exchangecode("XCEL");

   //request.set_producttype(com::celertech::marketdata::api::enums::producttype::ProductType::SPOT);
   request.set_producttype(bs::network::Asset::toCelerMDProductType(assetType_));
   request.set_assettype(bs::network::Asset::toCelerMDAssetType(assetType_));

   request.set_settlementtype("SP");

   logger_->debug("[CelerSubscribeToMDSequence::subscribeToMD] sending MarketDataSubscriptionRequest for {}"
      , currencyPair_);

   CelerMessage message;
   message.messageType = CelerAPI::MarketDataSubscriptionRequestType;
   message.messageData = request.SerializeAsString();

   return message;
}

CelerMessage CelerSubscribeToMDSequence::subscribeToMDStatistics()
{
   com::celertech::marketdata::api::marketstatistic::MarketStatisticRequest request;

   reqId_ = GetUniqueId();

   request.set_marketstatisticrequestid(reqId_);
   request.set_marketdatarequesttype(com::celertech::marketdata::api::enums::marketdatarequesttype::SNAPSHOT_PLUS_UPDATES);
   request.set_securitycode(currencyPair_);
   request.set_securityid(currencyPair_);
   request.set_producttype(bs::network::Asset::toCelerMDProductType(assetType_));
   request.set_assettype(bs::network::Asset::toCelerMDAssetType(assetType_));
   request.set_exchangecode("XCEL");

   logger_->debug("[CelerSubscribeToMDSequence::subscribeToMDStatistics] subscribe request : {}"
      , currencyPair_);

   CelerMessage message;
   message.messageType = CelerAPI::MarketStatisticRequestType;
   message.messageData = request.SerializeAsString();

   return message;
}