#ifndef __CELER_MARKET_DATA_PROCIDER_H__
#define __CELER_MARKET_DATA_PROCIDER_H__

#include "MarketDataProvider.h"

#include <atomic>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>

#include "CommonTypes.h"

#include "CelerCreateCCSecurityOnMDSequence.h"

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
      , const std::shared_ptr<spdlog::logger>& logger
      , bool filterUsdProducts);
   ~CelerMarketDataProvider() noexcept override = default;

   CelerMarketDataProvider(const CelerMarketDataProvider&) = delete;
   CelerMarketDataProvider& operator = (const CelerMarketDataProvider&) = delete;

   CelerMarketDataProvider(CelerMarketDataProvider&&) = delete;
   CelerMarketDataProvider& operator = (CelerMarketDataProvider&&) = delete;

   bool DisconnectFromMDSource() override;

   bool IsConnectionActive() const override;

   bool RegisterCCOnCeler(const std::string& securityId
      , const std::string& serverExchangeId);

protected:
   bool StartMDConnection() override;

public slots:
   void onCCSecurityReceived(const std::string& securityId);

signals:
   void CCSecuritRegistrationResult(bool result, const std::string& securityId);

private slots:
   void OnConnectedToCeler();
   void OnDisconnectedFromCeler();

private:
   void ConnectToCelerClient();

   bool onFullSnapshot(const std::string& data);
   bool onReqRejected(const std::string& data);
   bool onMDStatisticsUpdate(const std::string& data);

   static bool isPriceValid(double val);

   bool IsConnectedToCeler() const;

   bool SubscribeToCCProduct(const std::string& ccProduct);

   bool ProcessSecurityListingEvent(const std::string& data);

private:
   std::shared_ptr<ConnectionManager>  connectionManager_;
   std::shared_ptr<CelerClient>        celerClient_;

   std::unordered_map<std::string, std::string>    requests_;
   bool filterUsdProducts_;

   bool connectionToCelerCompleted_ = false;

   std::atomic_flag        ccSymbolsListLocker_ = ATOMIC_FLAG_INIT;
   std::set<std::string>   loadedSymbols_;
   std::set<std::string>   subscribedSymbols_;
};

#endif // __CELER_MARKET_DATA_PROCIDER_H__
