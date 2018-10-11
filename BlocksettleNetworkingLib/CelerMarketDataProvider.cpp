#include "CelerMarketDataProvider.h"

#include "CelerClient.h"
#include "CelerSubscribeToMDSequence.h"
#include "CommonTypes.h"
#include "ConnectionManager.h"
#include "CurrencyPair.h"
#include "EncryptionUtils.h"

#include <spdlog/spdlog.h>

#include "com/celertech/marketdata/api/price/DownstreamPriceProto.pb.h"

CelerMarketDataProvider::CelerMarketDataProvider(const std::shared_ptr<ConnectionManager>& connectionManager
      , const std::string& host, const std::string& port
      , const std::shared_ptr<spdlog::logger>& logger
      , bool filterUsdProducts)
 : MarketDataProvider(logger)
 , mdHost_{host}
 , mdPort_{port}
 , connectionManager_{connectionManager}
 , filterUsdProducts_{filterUsdProducts}
{
   celerClient_ = nullptr;
}

bool CelerMarketDataProvider::StartMDConnection()
{
   if (celerClient_ != nullptr) {
      logger_->error("[CelerMarketDataProvider::StartMDConnection] already connected.");
      return false;
   }

   emit StartConnecting();

   celerClient_ = std::make_shared<CelerClient>(connectionManager_, false);

   ConnectToCelerClient();

   const std::string credentials = SecureBinaryData().GenerateRandom(32).toHexStr();

   // login password could be any string
   if (!celerClient_->LoginToServer(mdHost_, mdPort_, credentials, credentials)) {
      logger_->error("[CelerMarketDataProvider::StartMDConnection] failed to connect to MD source");
      celerClient_ = nullptr;
      return false;
   }

   return true;
}

bool CelerMarketDataProvider::DisconnectFromMDSource()
{
   if (celerClient_ == nullptr) {
      logger_->debug("[CelerMarketDataProvider::DisconnectFromMDSource] already disconnected");
      return true;
   }

   emit Disconnecting();
   celerClient_->CloseConnection();
}

bool CelerMarketDataProvider::IsConnectionActive() const
{
   return celerClient_ != nullptr;
}

void CelerMarketDataProvider::ConnectToCelerClient()
{
   celerClient_->RegisterHandler(CelerAPI::MarketDataFullSnapshotDownstreamEventType, [this](const std::string& data) {
      return this->onFullSnapshot(data);
   });
   celerClient_->RegisterHandler(CelerAPI::MarketDataRequestRejectDownstreamEventType, [this](const std::string& data) {
      return this->onReqRejected(data);
   });

   connect(celerClient_.get(), &CelerClient::OnConnectedToServer, this, &CelerMarketDataProvider::OnConnectedToCeler);
   connect(celerClient_.get(), &CelerClient::OnConnectionClosed, this, &CelerMarketDataProvider::OnDisconnectedFromCeler);
}

void CelerMarketDataProvider::OnConnectedToCeler()
{
   std::vector<std::string> fxPairs{"EUR/GBP", "EUR/SEK", "GBP/SEK", "EUR/JPY", "GBP/JPY", "JPY/SEK"};
   const std::vector<std::string> xbtPairs{"XBT/GBP", "XBT/EUR", "XBT/SEK", "XBT/JPY"};

   if (!filterUsdProducts_) {
      fxPairs.emplace_back("EUR/USD");
   }

   for (const auto& ccyPair : fxPairs) {
      auto subscribeCommand = std::make_shared<CelerSubscribeToMDSequence>(ccyPair, bs::network::Asset::Type::SpotFX, logger_);
      if (!celerClient_->ExecuteSequence(subscribeCommand)) {
         logger_->error("[CelerMarketDataProvider::OnConnectedToCeler] failed to send subscribe to {}"
            , ccyPair);
      } else {
         emit MDSecurityReceived(ccyPair, {bs::network::Asset::Type::SpotFX});
         emit MDUpdate(bs::network::Asset::Type::SpotFX, QString::fromStdString(ccyPair), {});
      }
   }

   for (const auto& ccyPair : xbtPairs) {
      auto subscribeCommand = std::make_shared<CelerSubscribeToMDSequence>(ccyPair, bs::network::Asset::Type::SpotXBT, logger_);
      if (!celerClient_->ExecuteSequence(subscribeCommand)) {
         logger_->error("[CelerMarketDataProvider::OnConnectedToCeler] failed to send subscribe to {}"
            , ccyPair);
      } else {
         emit MDSecurityReceived(ccyPair, {bs::network::Asset::Type::SpotXBT});
         emit MDUpdate(bs::network::Asset::Type::SpotXBT, QString::fromStdString(ccyPair), {});
      }
   }

   emit MDSecuritiesReceived();
   emit Connected();
}

void CelerMarketDataProvider::OnDisconnectedFromCeler()
{
   emit Disconnecting();
   emit MDUpdate(bs::network::Asset::Undefined, QString(), {});

   celerClient_ = nullptr;
   emit Disconnected();
}

bool CelerMarketDataProvider::isPriceValid(double val)
{
   if ((val > 0) && (val == val)) {
      return true;
   }
   return false;
}

bool CelerMarketDataProvider::onFullSnapshot(const std::string& data)
{
   com::celertech::marketdata::api::price::MarketDataFullSnapshotDownstreamEvent response;

   if (!response.ParseFromString(data)) {
      logger_->error("[CelerMarketDataProvider::onFullSnapshot] Failed to parse MarketDataFullSnapshotDownstreamEvent");
      return false;
   }

   logger_->debug("[CelerMarketDataProvider::onFullSnapshot] {}", response.DebugString());

   auto security = QString::fromStdString(response.securitycode());
   if (security.isEmpty()) {
      security = QString::fromStdString(response.securityid());
   }

   bs::network::MDFields fields;

   for (int i=0; i < response.marketdatapricesnapshotlevel_size(); ++i) {
      const auto& levelPrice = response.marketdatapricesnapshotlevel(i);
      if (levelPrice.priceposition() == 1) {
         if (isPriceValid(levelPrice.entryprice())) {
            fields.emplace_back(bs::network::MDField{bs::network::MDField::fromCeler(levelPrice.marketdataentrytype())
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

bool CelerMarketDataProvider::onReqRejected(const std::string& data)
{
   com::celertech::marketdata::api::price::MarketDataRequestRejectDownstreamEvent response;
   if (!response.ParseFromString(data)) {
      logger_->error("[CelerMarketDataProvider::onReqRejected] Failed to parse MarketDataRequestRejectDownstreamEvent");
      return false;
   }

   logger_->debug("[CelerMarketDataProvider::onReqRejected] {}", response.DebugString());

   // text field contain rejected ccy pair
   emit MDReqRejected(response.text(), response.text());

   return true;
}
