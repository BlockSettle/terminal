#include "BSMarketDataProvider.h"

#include "TradeHistory.pb.h"
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
   Blocksettle::BS_MD::TradeHistoryServer::UpdateHeader header;

   if (!header.ParseFromString(data)) {
      logger_->error("[BSMarketDataProvider::onDataFromMD] failed to parse header");
      return ;
   }

   switch (header.type()) {
   case Blocksettle::BS_MD::TradeHistoryServer::FullSnapshotType:
      OnFullSnapshot(header.data());
      break;
   case Blocksettle::BS_MD::TradeHistoryServer::IncrementalUpdateType:
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
   emit MDUpdate(bs::network::Asset::Undefined, QString(), {});
   mdConnection_ = nullptr;
   emit Disconnected();
}

bs::network::MDFields GetMDFields(const Blocksettle::BS_MD::TradeHistoryServer::ProductHistoryInfo& productInfo)
{
   bs::network::MDFields result;

   if (productInfo.has_sell_price()) {
      result.emplace_back( bs::network::MDField{ bs::network::MDField::PriceBid, productInfo.sell_price().price(), QString()} );
   }
   if (productInfo.has_buy_price()) {
      result.emplace_back( bs::network::MDField{ bs::network::MDField::PriceOffer, productInfo.buy_price().price(), QString()} );
   }
   if (productInfo.has_last_price()) {
      result.emplace_back( bs::network::MDField{ bs::network::MDField::PriceLast, productInfo.last_price().price(), QString()} );
   }
   if (productInfo.has_volume()) {
      result.emplace_back( bs::network::MDField{ bs::network::MDField::DailyVolume, productInfo.volume().volume(), QString()} );
   }

   return result;
}

void BSMarketDataProvider::OnFullSnapshot(const std::string& data)
{
   Blocksettle::BS_MD::TradeHistoryServer::FullSnapshot snapshot;
   if (!snapshot.ParseFromString(data)) {
      logger_->error("[BSMarketDataProvider::OnFullSnapshot] failed to parse snapshot");
      return ;
   }

   for (int i=0; i < snapshot.fx_products_size(); ++i) {
      auto assetType = bs::network::Asset::Type::SpotFX;
      const auto& productInfo = snapshot.fx_products(i);
      emit MDSecurityReceived(productInfo.name(), {assetType});
      emit MDUpdate(assetType, QString::fromStdString(productInfo.name()), GetMDFields(productInfo));
   }

   for (int i=0; i < snapshot.xbt_products_size(); ++i) {
      auto assetType = bs::network::Asset::Type::SpotXBT;
      const auto& productInfo = snapshot.xbt_products(i);
      emit MDSecurityReceived(productInfo.name(), {assetType});
      emit MDUpdate(assetType, QString::fromStdString(productInfo.name()), GetMDFields(productInfo));
   }

   for (int i=0; i < snapshot.cc_products_size(); ++i) {
      auto assetType = bs::network::Asset::Type::PrivateMarket;
      const auto& productInfo = snapshot.cc_products(i);
      emit MDSecurityReceived(productInfo.name(), {assetType});
      emit MDUpdate(assetType, QString::fromStdString(productInfo.name()), GetMDFields(productInfo));
   }
}

void BSMarketDataProvider::OnIncrementalUpdate(const std::string& data)
{
   Blocksettle::BS_MD::TradeHistoryServer::IncrementalUpdate update;
   if (!update.ParseFromString(data)) {
      logger_->error("[BSMarketDataProvider::OnIncrementalUpdate] failed to parse update");
      return ;
   }

   bs::network::Asset::Type assetType;
   switch(update.group()) {
   case Blocksettle::BS_MD::TradeHistoryServer::FXProductGroup:
      assetType = bs::network::Asset::Type::SpotFX;
      break;
   case Blocksettle::BS_MD::TradeHistoryServer::XBTProductGroup:
      assetType = bs::network::Asset::Type::SpotXBT;
      break;
   case Blocksettle::BS_MD::TradeHistoryServer::CCProductGroup:
      assetType = bs::network::Asset::Type::PrivateMarket;
      break;
   }

   const auto& productInfo = update.update_info();
   emit MDUpdate(assetType, QString::fromStdString(productInfo.name()), GetMDFields(productInfo));
}