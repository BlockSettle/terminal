#ifndef __CELER_MARKET_DATA_PROCIDER_H__
#define __CELER_MARKET_DATA_PROCIDER_H__

#include "MarketDataProvider.h"

#include <memory>
#include <string>
#include <unordered_map>
#include "CommonTypes.h"

namespace spdlog
{
   class logger;
}

class CelerClient;
class ConnectionManager;

class CelerMarketDataProvider : public MarketDataProvider
{
Q_OBJECT

public:
   CelerMarketDataProvider(const std::shared_ptr<ConnectionManager>& connectionManager
      , const std::string& host, const std::string& port
      , const std::shared_ptr<spdlog::logger>& logger
      , bool filterUsdProducts);
   ~CelerMarketDataProvider() noexcept override = default;

   CelerMarketDataProvider(const CelerMarketDataProvider&) = delete;
   CelerMarketDataProvider& operator = (const CelerMarketDataProvider&) = delete;

   CelerMarketDataProvider(CelerMarketDataProvider&&) = delete;
   CelerMarketDataProvider& operator = (CelerMarketDataProvider&&) = delete;

   bool SubscribeToMD() override;
   bool DisconnectFromMDSource() override;

   bool IsConnectionActive() const override;

private slots:
   void OnConnectedToCeler();
   void OnDisconnectedFromCeler();

private:
   void ConnectToCelerClient();

   bool onFullSnapshot(const std::string& data);
   bool onReqRejected(const std::string& data);

   static bool isPriceValid(double val);

private:
   std::shared_ptr<spdlog::logger>     logger_;

   // connection details for MD source
   std::string mdHost_;
   std::string mdPort_;

   std::shared_ptr<ConnectionManager>  connectionManager_;
   std::shared_ptr<CelerClient>        celerClient_;

   std::unordered_map<std::string, std::string>    requests_;
   std::unordered_map<std::string, bs::network::SecurityDef>   secDefs_;
   bool filterUsdProducts_;
};

#endif // __CELER_MARKET_DATA_PROCIDER_H__