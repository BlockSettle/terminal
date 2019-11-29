/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __BS_MARKET_DATA_PROVIDER_H__
#define __BS_MARKET_DATA_PROVIDER_H__

#include "MarketDataProvider.h"

#include <memory>
#include <string>
#include <unordered_map>

#include "CommonTypes.h"

#include "bs_md.pb.h"

namespace spdlog
{
   class logger;
}

class ConnectionManager;
class SubscriberConnection;
class SubscriberConnectionListenerCB;

class BSMarketDataProvider : public MarketDataProvider
{
Q_OBJECT

public:
   BSMarketDataProvider(const std::shared_ptr<ConnectionManager>& connectionManager
      , const std::shared_ptr<spdlog::logger>& logger
      , bool receiveUSD = false);
   ~BSMarketDataProvider() noexcept override = default;

   BSMarketDataProvider(const BSMarketDataProvider&) = delete;
   BSMarketDataProvider& operator = (const BSMarketDataProvider&) = delete;

   BSMarketDataProvider(BSMarketDataProvider&&) = delete;
   BSMarketDataProvider& operator = (BSMarketDataProvider&&) = delete;

   bool DisconnectFromMDSource() override;

   bool IsConnectionActive() const override;

protected:
   bool StartMDConnection() override;
   void StopMDConnection() override;

private:
   void onDataFromMD(const std::string& data);
   void onConnectedToMD();
   void onDisconnectedFromMD();

   void OnFullSnapshot(const std::string& data);
   void OnIncrementalUpdate(const std::string& data);

   void OnNewTradeUpdate(const std::string& data);
   void OnNewFXTradeUpdate(const std::string& data);
   void OnNewXBTTradeUpdate(const std::string& data);
   void OnNewPMTradeUpdate(const std::string& data);

   void OnProductSnapshot(const bs::network::Asset::Type& assetType
      , const Blocksettle::Communication::BlocksettleMarketData::ProductPriceInfo& productInfo
      , double timestamp);
   void OnProductUpdate(const bs::network::Asset::Type& assetType
      , const Blocksettle::Communication::BlocksettleMarketData::ProductPriceInfo& productInfo
      , double timestamp);

private:
   std::shared_ptr<ConnectionManager>  connectionManager_;
   std::shared_ptr<SubscriberConnection> mdConnection_ = nullptr;
   std::shared_ptr<SubscriberConnectionListenerCB> listener_ = nullptr;

   const bool receiveUSD_ = false;
};

#endif // __BS_MARKET_DATA_PROVIDER_H__