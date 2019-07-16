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

   void SetConnectionSettings(const std::string &host, const std::string &port);

   void SubscribeToMD();
   void UnsubscribeFromMD();
   virtual bool DisconnectFromMDSource() { return true; }

   virtual bool IsConnectionActive() const { return false; }

protected:
   virtual bool StartMDConnection() { return true; }
   virtual void StopMDConnection() { }

public slots:
   void MDLicenseAccepted();

signals:
   void UserWantToConnectToMD();

   void WaitingForConnectionDetails();

   void StartConnecting();
   void Connected();

   void Disconnecting();
   void Disconnected();

   void MDUpdate(bs::network::Asset::Type, const QString &security, bs::network::MDFields);
   void MDSecurityReceived(const std::string &security, const bs::network::SecurityDef &sd);
   void MDSecuritiesReceived();
   void MDReqRejected(const std::string &security, const std::string &reason);

   void OnNewFXTrade(const bs::network::NewTrade& trade);
   void OnNewXBTTrade(const bs::network::NewTrade& trade);
   void OnNewPMTrade(const bs::network::NewPMTrade& trade);

protected:
   std::shared_ptr<spdlog::logger>  logger_;

   bool waitingForConnectionDetails_ = false;

   std::string host_;
   std::string port_;
};

#endif // __MARKET_DATA_PROVIDER_H__
