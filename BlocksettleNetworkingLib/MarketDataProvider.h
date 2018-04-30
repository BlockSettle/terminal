#ifndef __CELER_MARKET_DATA_PROVIDER_H__
#define __CELER_MARKET_DATA_PROVIDER_H__

#include <QObject>
#include <memory>
#include <string>
#include <unordered_map>
#include "CommonTypes.h"


namespace spdlog
{
   class logger;
}

class CelerClient;


class MarketDataProvider : public QObject
{
Q_OBJECT

public:
   MarketDataProvider(const std::shared_ptr<spdlog::logger>& logger);
   ~MarketDataProvider() noexcept = default;

   MarketDataProvider(const MarketDataProvider&) = delete;
   MarketDataProvider& operator = (const MarketDataProvider&) = delete;

   MarketDataProvider(MarketDataProvider&&) = delete;
   MarketDataProvider& operator = (MarketDataProvider&&) = delete;

   void ConnectToCelerClient(const std::shared_ptr<CelerClient>& celerClient, bool filterUsdProducts);

private slots:
   void OnConnectedToCeler();
   void OnDisconnectedFromCeler();

signals:
   void MDUpdate(bs::network::Asset::Type, const QString &security, bs::network::MDFields);
   void MDSecurityReceived(const std::string &security, const bs::network::SecurityDef &sd);
   void MDSecuritiesReceived();
   void MDReqRejected(const std::string &security, const std::string &reqson);

private:
   bool onMDUpdate(const std::string& data);
   bool onMDStats(const std::string &data);
   bool onReqRejected(const std::string& data);

   static bool isPriceValid(double val);

private:
   std::shared_ptr<spdlog::logger>  logger_;
   std::shared_ptr<CelerClient>     celerClient_;
   std::unordered_map<std::string, std::string>    requests_;
   std::unordered_map<std::string, bs::network::SecurityDef>   secDefs_;
   bool filterUsdProducts_;
};

#endif // __CELER_MARKET_DATA_PROVIDER_H__
