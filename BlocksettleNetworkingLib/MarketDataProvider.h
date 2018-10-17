#ifndef __MARKET_DATA_PROVIDER_H__
#define __MARKET_DATA_PROVIDER_H__

#include <QObject>

#include "CommonTypes.h"

#include <memory>
#include <string>
#include <unordered_map>

namespace spdlog
{
   class logger;
}

class MarketDataProvider : public QObject
{
Q_OBJECT

public:
   MarketDataProvider(const std::shared_ptr<spdlog::logger>& logger);
   ~MarketDataProvider() noexcept override = default;

   MarketDataProvider(const MarketDataProvider&) = delete;
   MarketDataProvider& operator = (const MarketDataProvider&) = delete;

   MarketDataProvider(MarketDataProvider&&) = delete;
   MarketDataProvider& operator = (MarketDataProvider&&) = delete;

   void SubscribeToMD();
   virtual bool DisconnectFromMDSource() = 0;

   virtual bool IsConnectionActive() const = 0;

protected:
   virtual bool StartMDConnection() = 0;

public slots:
   void MDLicenseAccepted();

signals:
   void UserWantToConnectToMD();

   void StartConnecting();
   void Connected();

   void Disconnecting();
   void Disconnected();

   void MDUpdate(bs::network::Asset::Type, const QString &security, bs::network::MDFields);
   void MDSecurityReceived(const std::string &security, const bs::network::SecurityDef &sd);
   void MDSecuritiesReceived();
   void MDReqRejected(const std::string &security, const std::string &reason);

protected:
   std::shared_ptr<spdlog::logger>  logger_ = nullptr;
};

#endif // __MARKET_DATA_PROVIDER_H__
