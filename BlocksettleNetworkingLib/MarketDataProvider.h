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
class ConnectionManager;

class MarketDataProvider : public QObject
{
Q_OBJECT

public:
   MarketDataProvider(const std::shared_ptr<ConnectionManager>& connectionManager
      , const std::string& host, const std::string& port
      , const std::shared_ptr<spdlog::logger>& logger);
   ~MarketDataProvider() noexcept = default;

   MarketDataProvider(const MarketDataProvider&) = delete;
   MarketDataProvider& operator = (const MarketDataProvider&) = delete;

   MarketDataProvider(MarketDataProvider&&) = delete;
   MarketDataProvider& operator = (MarketDataProvider&&) = delete;

   bool SubscribeToMD(bool filterUsdProducts);
   bool DisconnectFromMDSource();

   bool IsConnectionActive() const;

private slots:
   void OnConnectedToCeler();
   void OnDisconnectedFromCeler();

signals:
   void StartConnecting();
   void Connected();

   void Disconnecting();
   void Disconnected();

   void MDUpdate(bs::network::Asset::Type, const QString &security, bs::network::MDFields);
   void MDSecurityReceived(const std::string &security, const bs::network::SecurityDef &sd);
   void MDSecuritiesReceived();
   void MDReqRejected(const std::string &security, const std::string &reqson);

private:
   void ConnectToCelerClient();

   bool onMDUpdate(const std::string& data);
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

#endif // __CELER_MARKET_DATA_PROVIDER_H__
