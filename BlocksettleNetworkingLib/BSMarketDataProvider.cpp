#include "BSMarketDataProvider.h"

#include "ConnectionManager.h"
#include "SubscriberConnection.h"

#include <spdlog/spdlog.h>

#include <vector>

BSMarketDataProvider::BSMarketDataProvider(const std::shared_ptr<ConnectionManager>& connectionManager
      , const std::shared_ptr<spdlog::logger>& logger)
 : MarketDataProvider(logger)
 , connectionManager_{connectionManager}
{
}

bool BSMarketDataProvider::StartMDConnection()
{
   if (mdConnection_ != nullptr) {
      logger_->error("[BSMarketDataProvider::StartMDConnection] already connected");
      return false;
   }

   mdConnection_ = connectionManager_->CreateSubscriberConnection();

   auto onDataReceived = [this](const std::string& data) { this->onDataFromMD(data); };
   auto onConnectedToPb = [this]() { this->onConnectedToMD(); };
   auto onDisconnectedFromPB = [this]() { this->onDisconnectedFromMD(); };

   listener_ = std::make_shared<SubscriberConnectionListenerCB>(onDataReceived
      , onConnectedToPb, onDisconnectedFromPB);

   logger_->debug("[BSMarketDataProvider::StartMDConnection] start connecting to PB updates");

   emit StartConnecting();
   if (!mdConnection_->ConnectToPublisher(host_, port_, listener_.get())) {
      logger_->error("[BSMarketDataProvider::StartMDConnection] failed to start connection");
      emit Disconnected();
      return false;
   }

   return true;
}

bool BSMarketDataProvider::IsConnectionActive() const
{
   return mdConnection_ != nullptr;
}

bool BSMarketDataProvider::DisconnectFromMDSource()
{
   if (mdConnection_ == nullptr) {
      return true;
   }
   emit Disconnecting();

   mdConnection_->stopListen();

   return true;
}

void BSMarketDataProvider::onDataFromMD(const std::string& data)
{
   Blocksettle::Communication::BlocksettleMarketData::UpdateHeader header;

   if (!header.ParseFromString(data)) {
      logger_->error("[BSMarketDataProvider::onDataFromMD] failed to parse header");
      return ;
   }

   switch (header.type()) {
   case Blocksettle::Communication::BlocksettleMarketData::FullSnapshotType:
      OnFullSnapshot(header.data());
      break;
   case Blocksettle::Communication::BlocksettleMarketData::IncrementalUpdateType:
      OnIncrementalUpdate(header.data());
      break;
   }
}

void BSMarketDataProvider::onConnectedToMD()
{
   emit Connected();
}

void BSMarketDataProvider::onDisconnectedFromMD()
{
   emit Disconnecting();
   emit MDUpdate(bs::network::Asset::Undefined, QString(), {});

   mdConnection_ = nullptr;
   emit Disconnected();
}

bs::network::MDFields GetMDFields(const Blocksettle::Communication::BlocksettleMarketData::ProductPriceInfo& productInfo)
{
   bs::network::MDFields result;

   if (!qFuzzyIsNull(productInfo.offer())) {
      result.emplace_back( bs::network::MDField{ bs::network::MDField::PriceOffer, productInfo.offer(), QString()} );
   }
   if (!qFuzzyIsNull(productInfo.bid())) {
      result.emplace_back( bs::network::MDField{ bs::network::MDField::PriceBid, productInfo.bid(), QString()} );
   }
   if (!qFuzzyIsNull(productInfo.last_price())) {
      result.emplace_back( bs::network::MDField{ bs::network::MDField::PriceLast, productInfo.last_price(), QString()} );
   }
   if (!qFuzzyIsNull(productInfo.volume())) {
      result.emplace_back( bs::network::MDField{ bs::network::MDField::DailyVolume, productInfo.volume(), QString()} );
   }

   return result;
}

void BSMarketDataProvider::OnProductSnapshot(const bs::network::Asset::Type& assetType
   , const Blocksettle::Communication::BlocksettleMarketData::ProductPriceInfo& productInfo
   , double timestamp)
{
   emit MDSecurityReceived(productInfo.product_name(), {assetType});
   auto fields = GetMDFields(productInfo);
   fields.emplace_back(bs::network::MDField{bs::network::MDField::MDTimestamp, timestamp, {}});
   emit MDUpdate(assetType, QString::fromStdString(productInfo.product_name()), GetMDFields(productInfo));
}

void BSMarketDataProvider::OnFullSnapshot(const std::string& data)
{
   Blocksettle::Communication::BlocksettleMarketData::MDSnapshot snapshot;
   if (!snapshot.ParseFromString(data)) {
      logger_->error("[BSMarketDataProvider::OnFullSnapshot] failed to parse snapshot");
      return ;
   }

   double timestamp = static_cast<double>(snapshot.timestamp());

   for (int i=0; i < snapshot.fx_products_size(); ++i) {
      OnProductSnapshot(bs::network::Asset::Type::SpotFX, snapshot.fx_products(i), timestamp);
   }

   for (int i=0; i < snapshot.xbt_products_size(); ++i) {
      OnProductSnapshot(bs::network::Asset::Type::SpotXBT, snapshot.xbt_products(i), timestamp);
   }

   for (int i=0; i < snapshot.cc_products_size(); ++i) {
      OnProductSnapshot(bs::network::Asset::Type::PrivateMarket, snapshot.cc_products(i), timestamp);
   }
}

void BSMarketDataProvider::OnProductUpdate(const bs::network::Asset::Type& assetType
   , const Blocksettle::Communication::BlocksettleMarketData::ProductPriceInfo& productInfo
   , double timestamp)
{
   auto fields = GetMDFields(productInfo);
   if (!fields.empty()) {
      fields.emplace_back(bs::network::MDField{bs::network::MDField::MDTimestamp, timestamp, {}});
      emit MDUpdate(assetType, QString::fromStdString(productInfo.product_name()), GetMDFields(productInfo));
   }
}

void BSMarketDataProvider::OnIncrementalUpdate(const std::string& data)
{
   Blocksettle::Communication::BlocksettleMarketData::MDSnapshot update;
   if (!update.ParseFromString(data)) {
      logger_->error("[BSMarketDataProvider::OnIncrementalUpdate] failed to parse update");
      return ;
   }

   double timestamp = static_cast<double>(update.timestamp());

   for (int i=0; i < update.fx_products_size(); ++i) {
      OnProductUpdate(bs::network::Asset::Type::SpotFX, update.fx_products(i), timestamp);
   }

   for (int i=0; i < update.xbt_products_size(); ++i) {
      OnProductUpdate(bs::network::Asset::Type::SpotXBT, update.xbt_products(i), timestamp);
   }

   for (int i=0; i < update.cc_products_size(); ++i) {
      OnProductUpdate(bs::network::Asset::Type::PrivateMarket, update.cc_products(i), timestamp);
   }
}