/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "CelerMarketDataProvider.h"

#include "CelerClient.h"
#include "CelerLoadMDDefinitionsSequence.h"
#include "CelerSubscribeToMDSequence.h"
#include "CommonTypes.h"
#include "ConnectionManager.h"
#include "CurrencyPair.h"
#include "EncryptionUtils.h"
#include "FastLock.h"

#include <math.h>

#include <QDateTime>

#include <spdlog/spdlog.h>

#include "com/celertech/marketdata/api/price/DownstreamPriceProto.pb.h"
#include "com/celertech/marketdata/api/marketstatistic/DownstreamMarketStatisticProto.pb.h"
#include "com/celertech/staticdata/api/security/DownstreamSecurityProto.pb.h"

CelerMarketDataProvider::CelerMarketDataProvider(const std::shared_ptr<ConnectionManager>& connectionManager
      , const std::shared_ptr<spdlog::logger>& logger
      , bool filterUsdProducts)
 : MarketDataProvider(logger)
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

   celerClient_->RegisterHandler(CelerAPI::SecurityListingDownstreamEventType
      , [this](const std::string& data) { return ProcessSecurityListingEvent(data); });

   ConnectToCelerClient();

   const std::string credentials = CryptoPRNG::generateRandom(32).toHexStr();

   logger_->debug("[CelerMarketDataProvider::StartMDConnection] start connecting to {} : {}"
      , host_, port_);

   // login password could be any string
   if (!celerClient_->LoginToServer(host_, port_, credentials, credentials)) {
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

   return true;
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
   celerClient_->RegisterHandler(CelerAPI::MarketStatisticSnapshotDownstreamEventType, [this](const std::string& data) {
      return this->onMDStatisticsUpdate(data);
   });

   connect(celerClient_.get(), &CelerClient::OnConnectedToServer, this, &CelerMarketDataProvider::OnConnectedToCeler);
   connect(celerClient_.get(), &CelerClient::OnConnectionClosed, this, &CelerMarketDataProvider::OnDisconnectedFromCeler);
}

void CelerMarketDataProvider::OnConnectedToCeler()
{
   // load definitions for debug purpose
   auto loadDefinitions = std::make_shared<CelerLoadMDDefinitionsSequence>(logger_);
   if (!celerClient_->ExecuteSequence(loadDefinitions)) {
      logger_->debug("[CelerMarketDataProvider::OnConnectedToCeler] failed to send find request");
   }

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
         logger_->debug("[CelerMarketDataProvider::OnConnectedToCeler] add FX pair: {}", ccyPair);

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
         logger_->debug("[CelerMarketDataProvider::OnConnectedToCeler] add XBT pair: {}", ccyPair);

         emit MDSecurityReceived(ccyPair, {bs::network::Asset::Type::SpotXBT});
         emit MDUpdate(bs::network::Asset::Type::SpotXBT, QString::fromStdString(ccyPair), {});
      }
   }

   std::set<std::string> ccSymbols;
   {
      FastLock locker{ccSymbolsListLocker_};
      ccSymbols = loadedSymbols_;
   }

   for (const auto& ccSymbol : ccSymbols) {
      SubscribeToCCProduct(ccSymbol);
   }

   connectionToCelerCompleted_ = true;

   emit MDSecuritiesReceived();
   emit Connected();
}

void CelerMarketDataProvider::OnDisconnectedFromCeler()
{
   connectionToCelerCompleted_ = false;

   {
      FastLock locker{ccSymbolsListLocker_};
      subscribedSymbols_.clear();
   }

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

   auto security = QString::fromStdString(response.securitycode());
   if (security.isEmpty()) {
      security = QString::fromStdString(response.securityid());
   }

   if (!response.has_producttype()) {
      logger_->error("[CelerMarketDataProvider::onFullSnapshot] update do not have product type: {}"
         , response.DebugString());
      // we do not reject message, we just ignore illformed updated
      return true;
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

   const auto assetType = bs::network::Asset::fromCelerProductType(response.producttype());

   // we probably should not use celer timestamp
   fields.emplace_back(bs::network::MDField{bs::network::MDField::MDTimestamp, static_cast<double>(QDateTime::currentDateTime().toSecsSinceEpoch()), QString{}});

   emit MDUpdate(assetType, security, fields);

   return true;
}

bool CelerMarketDataProvider::onMDStatisticsUpdate(const std::string& data)
{
   com::celertech::marketdata::api::marketstatistic::MarketStatisticSnapshotDownstreamEvent response;

   if (!response.ParseFromString(data)) {
      logger_->error("[CelerMarketDataProvider::onMDStatisticsUpdate] Failed to parse MarketDataFullSnapshotDownstreamEvent");
      return false;
   }

   if (!response.has_snapshot()) {
      logger_->debug("[CelerMarketDataProvider::onMDStatisticsUpdate] empty snapshot");
      return true;
   }

   auto security = QString::fromStdString(response.securitycode());
   if (security.isEmpty()) {
      security = QString::fromStdString(response.securityid());
   }

   const auto assetType = bs::network::Asset::fromCelerProductType(response.producttype());

   bs::network::MDFields fields;

   const auto& snapshot = response.snapshot();

   if (snapshot.has_lastpx()) {
      const auto value = snapshot.lastpx();
      if (!std::isnan(value) && !qFuzzyIsNull(value)) {
         fields.emplace_back(bs::network::MDField{bs::network::MDField::PriceLast
               , value, QString()});
      }
   }

   if (snapshot.has_dailyvolume()) {
      const auto value = snapshot.dailyvolume();
      if (!std::isnan(value) && !qFuzzyIsNull(value)) {
         fields.emplace_back(bs::network::MDField{bs::network::MDField::DailyVolume
               , value, QString()});
      }
   }

   if (!fields.empty()) {
      fields.emplace_back(bs::network::MDField{bs::network::MDField::MDTimestamp
         , static_cast<double>(QDateTime::currentDateTime().toSecsSinceEpoch()), {}});
      emit MDUpdate(assetType, security, fields);
   }

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

   {
      FastLock locker{ccSymbolsListLocker_};
      auto it = subscribedSymbols_.find(response.securityid());
      if (it != subscribedSymbols_.end()) {
         subscribedSymbols_.erase(it);
      }
   }

   emit MDReqRejected(response.securityid(), response.text());

   return true;
}

bool CelerMarketDataProvider::RegisterCCOnCeler(const std::string& securityId
   , const std::string& serverExchangeId)
{
   if (!IsConnectedToCeler()) {
      logger_->error("[CelerMarketDataProvider::RegisterCCOnCeler] can't register CC product while disconnected");
      return false;
   }

   auto command = std::make_shared<CelerCreateCCSecurityOnMDSequence>(securityId
      , serverExchangeId, logger_);

   logger_->debug("[CelerMarketDataProvider::RegisterCCOnCeler] registering CC on celer MD: {}"
      , securityId);

   if (!celerClient_->ExecuteSequence(command)) {
      logger_->error("[CelerMarketDataProvider::RegisterCCOnCeler] failed to send command to MD server for {}"
         , securityId);
      return false;
   }

   return true;
}

void CelerMarketDataProvider::onCCSecurityReceived(const std::string& securityId)
{
   logger_->debug("[CelerMarketDataProvider::onCCSecurityReceived] loaded CC symbol {}"
      , securityId);

   {
      FastLock locker{ccSymbolsListLocker_};
      if (loadedSymbols_.find(securityId) != loadedSymbols_.end()) {
         logger_->debug("[CelerMarketDataProvider::onCCSecurityReceived] {} already loaded. ignore"
            , securityId);
         return;
      }

      loadedSymbols_.emplace(securityId);
   }

   if (IsConnectedToCeler()) {
      if (SubscribeToCCProduct(securityId)) {
         emit MDSecuritiesReceived();
      }
   }
}

bool CelerMarketDataProvider::IsConnectedToCeler() const
{
   return connectionToCelerCompleted_;
}

bool CelerMarketDataProvider::SubscribeToCCProduct(const std::string& ccProduct)
{
   {
      FastLock locker{ccSymbolsListLocker_};
      if (subscribedSymbols_.find(ccProduct) != subscribedSymbols_.end()) {
         logger_->debug("[CelerMarketDataProvider::SubscribeToCCProduct] already subscribed to {}"
            , ccProduct);
         return true;
      }
   }

   auto subscribeCommand = std::make_shared<CelerSubscribeToMDSequence>(ccProduct
      , bs::network::Asset::Type::PrivateMarket, logger_);

   if (!celerClient_->ExecuteSequence(subscribeCommand)) {
      logger_->error("[CelerMarketDataProvider::SubscribeToCCProduct] failed to send subscribe to {}"
         , ccProduct);
      return false;
   }

   {
      FastLock locker{ccSymbolsListLocker_};
      subscribedSymbols_.emplace(ccProduct);
   }

   emit MDSecurityReceived(ccProduct, {bs::network::Asset::Type::PrivateMarket});
   emit MDUpdate(bs::network::Asset::Type::PrivateMarket, QString::fromStdString(ccProduct), {});

   return true;
}

bool CelerMarketDataProvider::ProcessSecurityListingEvent(const std::string& data)
{
   com::celertech::staticdata::api::security::SecurityListingDownstreamEvent responseEvent;
   if (!responseEvent.ParseFromString(data)) {
      logger_->error("[CelerMarketDataProvider::ProcessSecurityListingEvent] failed to parse SecurityListingDownstreamEvent");
      return false;
   }

   logger_->debug("[CelerMarketDataProvider::ProcessSecurityListingEvent] get confirmation for {}"
                  , responseEvent.securityid());

   emit CCSecuritRegistrationResult(true, responseEvent.securityid());

   return true;
}
