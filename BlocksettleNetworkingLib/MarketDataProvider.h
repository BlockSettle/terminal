#ifndef __MARKET_DATA_PROVIDER_H__
#define __MARKET_DATA_PROVIDER_H__

#include <QObject>
#include <memory>
#include <string>
#include <unordered_map>
#include "CommonTypes.h"

class MarketDataProvider : public QObject
{
Q_OBJECT

public:
   MarketDataProvider() = default;
   ~MarketDataProvider() noexcept override = default;

   MarketDataProvider(const MarketDataProvider&) = delete;
   MarketDataProvider& operator = (const MarketDataProvider&) = delete;

   MarketDataProvider(MarketDataProvider&&) = delete;
   MarketDataProvider& operator = (MarketDataProvider&&) = delete;

   virtual bool SubscribeToMD() = 0;
   virtual bool DisconnectFromMDSource() = 0;

   virtual bool IsConnectionActive() const = 0;

signals:
   void StartConnecting();
   void Connected();

   void Disconnecting();
   void Disconnected();

   void MDUpdate(bs::network::Asset::Type, const QString &security, bs::network::MDFields);
   void MDSecurityReceived(const std::string &security, const bs::network::SecurityDef &sd);
   void MDSecuritiesReceived();
   void MDReqRejected(const std::string &security, const std::string &reqson);
};

#endif // __MARKET_DATA_PROVIDER_H__
