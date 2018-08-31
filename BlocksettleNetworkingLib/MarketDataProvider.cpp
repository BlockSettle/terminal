#include "MarketDataProvider.h"

#include "CelerClient.h"
#include "CelerSubscribeToMDSequence.h"
#include "CommonTypes.h"
#include "ConnectionManager.h"
#include "CurrencyPair.h"

#include <spdlog/spdlog.h>

#include "com/celertech/marketdata/api/price/DownstreamPriceProto.pb.h"

MarketDataProvider::MarketDataProvider(const std::shared_ptr<ConnectionManager>& connectionManager
      , const std::string& host, const std::string& port
      , const std::shared_ptr<spdlog::logger>& logger)
 : logger_(logger)
 , mdHost_{host}
 , mdPort_{port}
 , connectionManager_{connectionManager}
 , filterUsdProducts_(true)
{
   celerClient_ = nullptr;
}

bool MarketDataProvider::SubscribeToMD(bool filterUsdProducts)
{
   if (celerClient_ != nullptr) {
      logger_->error("[MarketDataProvider::SubscribeToMD] already connected.");
      return false;
   }

   emit StartConnecting();

   celerClient_ = std::make_shared<CelerClient>(connectionManager_, false);
   filterUsdProducts_ = filterUsdProducts;

   ConnectToCelerClient();

   // login password could be any string
   if (!celerClient_->LoginToServer(mdHost_, mdPort_, "pb_uat", "pb_uatpb_uat")) {
      logger_->error("[MarketDataProvider::SubscribeToMD] failed to connect to MD source");
      celerClient_ = nullptr;
      return false;
   }

   return true;
}

bool MarketDataProvider::DisconnectFromMDSource()
{
   if (celerClient_ == nullptr) {
      logger_->debug("[MarketDataProvider::DisconnectFromMDSource] already disconnected");
      return true;
   }

   emit Disconnecting();
   celerClient_->CloseConnection();
}

bool MarketDataProvider::IsConnectionActive() const
{
   return celerClient_ != nullptr;
}

void MarketDataProvider::ConnectToCelerClient()
{
   celerClient_->RegisterHandler(CelerAPI::MarketDataFullSnapshotDownstreamEventType, [this](const std::string& data) {
      return this->onFullSnapshot(data);
   });
   celerClient_->RegisterHandler(CelerAPI::MarketDataIncrementalDownstreamEventType, [this](const std::string& data) {
      return onIncrementalUpdate(data);
   });
   celerClient_->RegisterHandler(CelerAPI::MarketDataRequestRejectDownstreamEventType, [this](const std::string& data) {
      return this->onReqRejected(data);
   });

   connect(celerClient_.get(), &CelerClient::OnConnectedToServer, this, &MarketDataProvider::OnConnectedToCeler);
   connect(celerClient_.get(), &CelerClient::OnConnectionClosed, this, &MarketDataProvider::OnDisconnectedFromCeler);
}

void MarketDataProvider::OnConnectedToCeler()
{
   std::vector<std::string> fxPairs{"EUR/SEK", "EUR/GBP", "GBP/SEK"};
   const std::vector<std::string> xbtPairs{"XBT/SEK", "XBT/GBP", "XBT/EUR"};

   if (!filterUsdProducts_) {
      fxPairs.emplace_back("EUR/USD");
   }

   for (const auto& ccyPair : fxPairs) {
      auto subscribeCommand = std::make_shared<CelerSubscribeToMDSequence>(ccyPair, bs::network::Asset::Type::SpotFX, logger_);
      if (!celerClient_->ExecuteSequence(subscribeCommand)) {
         logger_->error("[MarketDataProvider::OnConnectedToCeler] failed to send subscribe to {}"
            , ccyPair);
      } else {
         emit MDSecurityReceived(ccyPair, {bs::network::Asset::Type::SpotFX});
         emit MDUpdate(bs::network::Asset::Type::SpotFX, QString::fromStdString(ccyPair), {});
      }
   }

   for (const auto& ccyPair : xbtPairs) {
      auto subscribeCommand = std::make_shared<CelerSubscribeToMDSequence>(ccyPair, bs::network::Asset::Type::SpotXBT, logger_);
      if (!celerClient_->ExecuteSequence(subscribeCommand)) {
         logger_->error("[MarketDataProvider::OnConnectedToCeler] failed to send subscribe to {}"
            , ccyPair);
      } else {
         emit MDSecurityReceived(ccyPair, {bs::network::Asset::Type::SpotXBT});
         emit MDUpdate(bs::network::Asset::Type::SpotXBT, QString::fromStdString(ccyPair), {});
      }
   }

   emit MDSecuritiesReceived();
   emit Connected();
}

void MarketDataProvider::OnDisconnectedFromCeler()
{
   emit Disconnecting();
   emit MDUpdate(bs::network::Asset::Undefined, QString(), {});

   celerClient_ = nullptr;
   emit Disconnected();
}

bool MarketDataProvider::isPriceValid(double val)
{
   if ((val > 0) && (val == val)) {
      return true;
   }
   return false;
}

bool MarketDataProvider::onIncrementalUpdate(const std::string& data)
{
   com::celertech::marketdata::api::price::MarketDataIncrementalDownstreamEvent response;

   if (!response.ParseFromString(data)) {
      logger_->error("[MarketDataProvider::onIncrementalUpdate] Failed to parse MarketDataIncrementalDownstreamEvent");
      return false;
   }

   logger_->debug("[MarketDataProvider::onIncrementalUpdate] {}", response.DebugString());

   // if (!response.ParseFromString(data)) {
   //       logger_->error("[MarketDataProvider::onMDUpdate] Failed to parse MarketDataFullSnapshotDownstreamEvent");
   //       return false;
   // }

   // auto security = QString::fromStdString(response.securitycode());
   // if (security.isEmpty()) {
   //    security = QString::fromStdString(response.securityid());
   // }

   // bs::network::MDFields fields;

   // for (int i=0; i < response.marketdatapricesnapshotlevel_size(); ++i) {
   //    const auto& levelPrice = response.marketdatapricesnapshotlevel(i);
   //    if (levelPrice.entryposition() == 1) {
   //       if (isPriceValid(levelPrice.entryprice())) {
   //          fields.push_back({bs::network::MDField::fromCeler(levelPrice.marketdataentrytype())
   //             , levelPrice.entryprice(), QString()});
   //       }
   //    }
   // }

   // const auto itSecDef = secDefs_.find(security.toStdString());
   // const auto assetType = (itSecDef == secDefs_.end()) ? bs::network::Asset::fromCelerProductType(response.producttype())
   //    : itSecDef->second.assetType;
   // emit MDUpdate(assetType, security, fields);

   return true;
}

bool MarketDataProvider::onFullSnapshot(const std::string& data)
{
   com::celertech::marketdata::api::price::MarketDataFullSnapshotDownstreamEvent response;

   if (!response.ParseFromString(data)) {
      logger_->error("[MarketDataProvider::onFullSnapshot] Failed to parse MarketDataFullSnapshotDownstreamEvent");
      return false;
   }

   logger_->debug("[MarketDataProvider::onFullSnapshot] {}", response.DebugString());

   // auto security = QString::fromStdString(response.securitycode());
   // if (security.isEmpty()) {
   //    security = QString::fromStdString(response.securityid());
   // }

   // bs::network::MDFields fields;

   // for (int i=0; i < response.marketdatapricesnapshotlevel_size(); ++i) {
   //    const auto& levelPrice = response.marketdatapricesnapshotlevel(i);
   //    if (levelPrice.entryposition() == 1) {
   //       if (isPriceValid(levelPrice.entryprice())) {
   //          fields.push_back({bs::network::MDField::fromCeler(levelPrice.marketdataentrytype())
   //             , levelPrice.entryprice(), QString()});
   //       }
   //    }
   // }

   // const auto itSecDef = secDefs_.find(security.toStdString());
   // const auto assetType = (itSecDef == secDefs_.end()) ? bs::network::Asset::fromCelerProductType(response.producttype())
   //    : itSecDef->second.assetType;
   // emit MDUpdate(assetType, security, fields);

   return true;
}

bool MarketDataProvider::onReqRejected(const std::string& data)
{
   com::celertech::marketdata::api::price::MarketDataRequestRejectDownstreamEvent response;
   if (!response.ParseFromString(data)) {
      logger_->error("[MarketDataProvider::onReqRejected] Failed to parse MarketDataRequestRejectDownstreamEvent");
      return false;
   }

   logger_->debug("[MarketDataProvider::onReqRejected] {}", response.DebugString());

   // text field contain rejected ccy pair
   emit MDReqRejected(response.text(), "rejected");

   return true;
}
