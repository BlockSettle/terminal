#ifndef __BS_MARKET_DATA_PROVIDER_H__
#define __BS_MARKET_DATA_PROVIDER_H__

#include "MarketDataProvider.h"

#include <memory>
#include <string>
#include <unordered_map>

#include "CommonTypes.h"

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
      , const std::shared_ptr<spdlog::logger>& logger);
   ~BSMarketDataProvider() noexcept override = default;

   BSMarketDataProvider(const BSMarketDataProvider&) = delete;
   BSMarketDataProvider& operator = (const BSMarketDataProvider&) = delete;

   BSMarketDataProvider(BSMarketDataProvider&&) = delete;
   BSMarketDataProvider& operator = (BSMarketDataProvider&&) = delete;

   bool DisconnectFromMDSource() override;

   bool IsConnectionActive() const override;

protected:
   bool StartMDConnection() override;

private:
   void onDataFromMD(const std::string& data);
   void onConnectedToMD();
   void onDisconnectedFromMD();

   void OnFullSnapshot(const std::string& data);
   void OnIncrementalUpdate(const std::string& data);

private:
   std::shared_ptr<ConnectionManager>  connectionManager_;
   std::shared_ptr<SubscriberConnection> mdConnection_ = nullptr;
   std::shared_ptr<SubscriberConnectionListenerCB> listener_ = nullptr;
};

#endif // __BS_MARKET_DATA_PROVIDER_H__