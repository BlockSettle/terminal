#include "MarketDataProvider.h"

#include "CelerClient.h"
#include "CelerSubscribeToMDSequence.h"
#include "CelerSubscribeToSecurities.h"
#include "CommonTypes.h"
#include "CurrencyPair.h"

#include <spdlog/spdlog.h>

#include "DownstreamMarketDataProto.pb.h"
#include "DownstreamMarketStatisticProto.pb.h"


using namespace com::celertech::marketmerchant::api::marketdata;
using namespace com::celertech::marketmerchant::api::marketstatistic;


MarketDataProvider::MarketDataProvider(const std::shared_ptr<spdlog::logger>& logger)
 : logger_(logger)
 , filterUsdProducts_(true)
{
   qRegisterMetaType<bs::network::Asset::Type>("AssetType");
   qRegisterMetaType<bs::network::SecurityDef>("SecurityDef");
   qRegisterMetaType<bs::network::MDField>("MDField");
   qRegisterMetaType<bs::network::MDFields>("MDFields");
}

void MarketDataProvider::ConnectToCelerClient(const std::shared_ptr<CelerClient>& celerClient, bool filterUsdProducts)
{
   celerClient_ = celerClient;
   filterUsdProducts_ = filterUsdProducts;

   celerClient->RegisterHandler(CelerAPI::MarketDataFullSnapshotDownstreamEventType, [this](const std::string& data) {
      return this->onMDUpdate(data);
   });
   celerClient->RegisterHandler(CelerAPI::MarketDataIncrementalDownstreamEventType, [this](const std::string& data) {
      return onMDUpdate(data);
   });
   celerClient->RegisterHandler(CelerAPI::MarketStatsSnapshotEventType, [this](const std::string& data) {
      return onMDStats(data);
   });
   celerClient->RegisterHandler(CelerAPI::MarketDataRequestRejectType, [this](const std::string& data) {
      return this->onReqRejected(data);
   });

   connect(celerClient.get(), &CelerClient::OnConnectedToServer, this, &MarketDataProvider::OnConnectedToCeler);
   connect(celerClient.get(), &CelerClient::OnConnectionClosed, this, &MarketDataProvider::OnDisconnectedFromCeler);
}

void MarketDataProvider::OnConnectedToCeler()
{
   auto onSecuritiesReceived = [this](const bs::network::CelerSubscribeToSecurities::Securities &securities) {
      for (auto security : securities) {
         if (filterUsdProducts_) {
            CurrencyPair pair{security.first};
            if ((pair.NumCurrency() == "USD") || (pair.DenomCurrency() == "USD")) {
               continue;
            }
         }

         auto mdReq = std::make_shared<CelerSubscribeToMDSequence>(security.first, security.second.assetType, logger_);
         if (!celerClient_->ExecuteSequence(mdReq)) {
            logger_->error("[MarketDataProvider::OnConnectedToCeler] failed to execut MD request to {}", security.first);
         }
         else {
            requests_[mdReq->getReqId()] = security.first;
            secDefs_[security.first] = security.second;
         }
         emit MDSecurityReceived(security.first, security.second);
         emit MDUpdate(security.second.assetType, QString::fromStdString(security.first), {});
      }
      emit MDSecuritiesReceived();
   };

   auto dicoReq = std::make_shared<bs::network::CelerSubscribeToSecurities>(logger_, onSecuritiesReceived);
   if (!celerClient_->ExecuteSequence(dicoReq)) {
      logger_->error("[MarketDataProvider::OnConnectedToCeler] failed to execute CelerSubscribeToSecurities");
   }
   else {
      logger_->debug("[MarketDataProvider::OnConnectedToCeler] requested securities");
   }
}

void MarketDataProvider::OnDisconnectedFromCeler()
{
   emit MDUpdate(bs::network::Asset::Undefined, QString(), {});
}

bool MarketDataProvider::isPriceValid(double val)
{
   if ((val > 0) && (val == val)) {
      return true;
   }
   return false;
}

bool MarketDataProvider::onMDUpdate(const std::string& data)
{
   MarketDataFullSnapshotDownstreamEvent response;

   if (!response.ParseFromString(data)) {
         logger_->error("[MarketDataProvider::onMDUpdate] Failed to parse MarketDataFullSnapshotDownstreamEvent");
         return false;
   }

   auto security = QString::fromStdString(response.securitycode());
   if (security.isEmpty()) {
      security = QString::fromStdString(response.securityid());
   }
//   logger_->debug("[MDUpdate] {}", response.DebugString());

   bs::network::MDFields fields;

   for (int i=0; i < response.marketdatapricesnapshotlevel_size(); ++i) {
      const auto& levelPrice = response.marketdatapricesnapshotlevel(i);
      if (levelPrice.entryposition() == 1) {
         if (isPriceValid(levelPrice.entryprice())) {
            fields.push_back({bs::network::MDField::fromCeler(levelPrice.marketdataentrytype())
               , levelPrice.entryprice(), QString()});
         }
      }
   }

   const auto itSecDef = secDefs_.find(security.toStdString());
   const auto assetType = (itSecDef == secDefs_.end()) ? bs::network::Asset::fromCelerProductType(response.producttype())
      : itSecDef->second.assetType;
   emit MDUpdate(assetType, security, fields);

   return true;
}

bool MarketDataProvider::onMDStats(const std::string &data)
{
   MarketStatisticSnapshotDownstreamEvent response;
   if (!response.ParseFromString(data)) {
      logger_->error("[MarketDataProvider::onMDUpdate] Failed to parse MarketStatisticSnapshot");
      return false;
   }
//   logger_->debug("MDStats: {}", response.DebugString());

   auto security = QString::fromStdString(response.securitycode());
   if (security.isEmpty()) {
      security = QString::fromStdString(response.securityid());
   }

   const auto snapshot = response.snapshot();
   bs::network::MDFields fields;

   if (isPriceValid(snapshot.lastpx())) {
      fields.push_back({bs::network::MDField::PriceLast, snapshot.lastpx(), QString() });
   }
   if (isPriceValid(snapshot.bestbidpx())) {
      fields.push_back({ bs::network::MDField::PriceBestBid, snapshot.bestbidpx(), QString() });
   }
   if (isPriceValid(snapshot.bestofferpx())) {
      fields.push_back({ bs::network::MDField::PriceBestOffer, snapshot.bestofferpx(), QString() });
   }
   if (isPriceValid(snapshot.dailyvolume())) {
      fields.push_back({ bs::network::MDField::DailyVolume, snapshot.dailyvolume(), QString() });
   }

   if (!fields.empty()) {
      const auto itSecDef = secDefs_.find(security.toStdString());
      const auto assetType = (itSecDef == secDefs_.end()) ? bs::network::Asset::fromCelerProductType(response.producttype())
         : itSecDef->second.assetType;
      emit MDUpdate(assetType, security, fields);
   }
   return true;
}

bool MarketDataProvider::onReqRejected(const std::string& data)
{
   MarketDataRequestRejectDownstreamEvent response;
   if (!response.ParseFromString(data)) {
      logger_->error("[MarketDataProvider::onReqRejected] Failed to parse MarketDataRequestRejectDownstreamEvent");
      return false;
   }
   emit MDReqRejected(requests_[response.marketdatarequestid()], response.text());
   logger_->warn("[MDReject] {}", response.DebugString());
   return true;
}
