/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __USER_SCRIPT_H__
#define __USER_SCRIPT_H__

#include <QQmlEngine>
#include <memory>
#include "CommonTypes.h"

#include <map>

namespace spdlog {
   class logger;
}
namespace bs {
   namespace sync {
      class WalletsManager;
   }
}
class QQmlComponent;
class AssetManager;
class MarketDataProvider;


//
// MarketData
//

//! Market data for user script.
class MarketData : public QObject
{
   Q_OBJECT

public:
   MarketData(std::shared_ptr<MarketDataProvider> mdProvider, QObject *parent);
   ~MarketData() noexcept override = default;

   Q_INVOKABLE double bid(const QString &sec) const;
   Q_INVOKABLE double ask(const QString &sec) const;

private slots:
   void onMDUpdated(bs::network::Asset::Type, const QString &security, bs::network::MDFields);

private:
   std::map<QString, std::map<bs::network::MDField::Type, double>> data_;
}; // class MarketData


//
// DataStorage
//

//! Data storage for user script.
class DataStorage : public QObject
{
   Q_OBJECT

public:
   explicit DataStorage(QObject *parent);
   ~DataStorage() noexcept override = default;

   Q_INVOKABLE double bought(const QString &currency);
   Q_INVOKABLE void setBought(const QString &currency, double v, const QString &id);

   Q_INVOKABLE double sold(const QString &currency);
   Q_INVOKABLE void setSold(const QString &currency, double v, const QString &id);

private:
   std::map<QString, std::map<QString, double>> bought_;
   std::map<QString, std::map<QString, double>> sold_;
}; // class DataStorage


//
// Constants
//

//! Useful constants for user script.
class Constants : public QObject
{
   Q_OBJECT

   Q_PROPERTY(int payInTxSize READ payInTxSize)    // TODO: turn these to payInFeeEstimate and payOutFeeEstimate
   Q_PROPERTY(int payOutTxSize READ payOutTxSize)
   Q_PROPERTY(float feePerByte READ feePerByte)
   Q_PROPERTY(QString xbtProductName READ xbtProductName)

public:
   explicit Constants(QObject *parent);
   ~Constants() noexcept override = default;

   int payInTxSize() const;
   int payOutTxSize() const;
   float feePerByte();
   QString xbtProductName() const;

   void setWalletsManager(std::shared_ptr<bs::sync::WalletsManager> walletsManager);

private:
   std::shared_ptr<bs::sync::WalletsManager> walletsManager_;
   mutable float feePerByte_ = 0.0;
}; // class Constants


class UserScript : public QObject
{
Q_OBJECT

public:
   UserScript(const std::shared_ptr<spdlog::logger> logger,
      std::shared_ptr<MarketDataProvider> mdProvider,
      QObject* parent = nullptr);
   ~UserScript() override;

   void load(const QString &filename);
   QObject *instantiate();

   void setWalletsManager(std::shared_ptr<bs::sync::WalletsManager> walletsManager);

signals:
   void loaded();
   void failed(const QString &desc);

private:
   std::shared_ptr<spdlog::logger> logger_;
   QQmlEngine *engine_;
   QQmlComponent *component_;
   MarketData *md_;
   Constants *const_;
   DataStorage *storage_;
};


class AutoQuoter : public QObject
{
Q_OBJECT

public:
   AutoQuoter(const std::shared_ptr<spdlog::logger> logger, const QString &filename,
      const std::shared_ptr<AssetManager> &assetManager,
      std::shared_ptr<MarketDataProvider> mdProvider,
      QObject* parent = nullptr);
   ~AutoQuoter() override = default;

   QObject *instantiate(const bs::network::QuoteReqNotification &qrn);
   void destroy(QObject *);

   void setWalletsManager(std::shared_ptr<bs::sync::WalletsManager> walletsManager);

signals:
   void loaded();
   void failed(const QString &desc);
   void sendingQuoteReply(const QString &reqId, double price);
   void pullingQuoteReply(const QString &reqId);

private:
   UserScript  script_;
   std::shared_ptr<spdlog::logger> logger_;
   std::shared_ptr<AssetManager> assetManager_;
};


class BSQuoteRequest : public QObject
{  // Nested class for BSQuoteReqReply - not instantiatable
   Q_OBJECT
   Q_PROPERTY(QString requestId READ requestId)
   Q_PROPERTY(QString product READ product)
   Q_PROPERTY(bool isBuy READ isBuy)
   Q_PROPERTY(double quantity READ quantity)
   Q_PROPERTY(int assetType READ assetType)

public:
   explicit BSQuoteRequest(QObject *parent = nullptr) : QObject(parent) {}
   ~BSQuoteRequest() override = default;
   void init(const QString &reqId, const QString &product, bool buy, double qty, int at);

   QString requestId() const  { return requestId_; }
   QString product() const    { return product_; }
   bool isBuy() const         { return isBuy_; }
   double quantity() const    { return quantity_; }
   int assetType() const      { return assetType_; }

   enum AssetType {
      SpotFX = 1,
      SpotXBT,
      PrivateMarket
   };

   Q_ENUM(AssetType)

private:
   QString  requestId_;
   QString  product_;
   bool     isBuy_;
   double   quantity_;
   int      assetType_;
};

class BSQuoteReqReply : public QObject
{     // Main QML-script facing class
   Q_OBJECT
   Q_PROPERTY(BSQuoteRequest* quoteReq READ quoteReq)
   Q_PROPERTY(double expirationInSec READ expiration WRITE setExpiration NOTIFY expirationInSecChanged)
   Q_PROPERTY(QString security READ security)
   Q_PROPERTY(double indicBid READ indicBid NOTIFY indicBidChanged)
   Q_PROPERTY(double indicAsk READ indicAsk NOTIFY indicAskChanged)
   Q_PROPERTY(double lastPirce READ lastPrice NOTIFY lastPriceChanged)
   Q_PROPERTY(double bestPrice READ bestPrice NOTIFY bestPriceChanged)

public:
   explicit BSQuoteReqReply(QObject *parent = nullptr) : QObject(parent) {}   //TODO: add dedicated AQ bs::Wallet
   ~BSQuoteReqReply() override = default;

   void setQuoteReq(BSQuoteRequest *qr)   { quoteReq_ = qr; }
   BSQuoteRequest *quoteReq() const       { return quoteReq_; }

   void setExpiration(double exp) {
      if (exp != expirationInSec_) {
         expirationInSec_ = exp;
         emit expirationInSecChanged();
      }
   }
   double expiration() const { return expirationInSec_; }

   void setSecurity(const QString &security) { security_ = security; }
   QString security() const { return security_; }

   void setIndicBid(double prc) {
      if (indicBid_ != prc) {
         indicBid_ = prc;
         emit indicBidChanged();
      }
   }
   void setIndicAsk(double prc) {
      if (indicAsk_ != prc) {
         indicAsk_ = prc;
         emit indicAskChanged();
      }
   }
   void setLastPrice(double prc) {
      if (lastPrice_ != prc) {
         lastPrice_ = prc;
         emit lastPriceChanged();
      }
   }
   void setBestPrice(double prc) {
      if (bestPrice_ != prc) {
         bestPrice_ = prc;
         emit bestPriceChanged();
      }
   }
   double indicBid() const { return indicBid_; }
   double indicAsk() const { return indicAsk_; }
   double lastPrice() const { return lastPrice_; }
   double bestPrice() const { return bestPrice_; }

   void init(const std::shared_ptr<spdlog::logger> &logger, const std::shared_ptr<AssetManager> &assetManager);

   Q_INVOKABLE void log(const QString &);
   Q_INVOKABLE bool sendQuoteReply(double price);
   Q_INVOKABLE bool pullQuoteReply();
   Q_INVOKABLE QString product();
   Q_INVOKABLE double accountBalance(const QString &product);
   void start() { emit started(); }

signals:
   void expirationInSecChanged();
   void indicBidChanged();
   void indicAskChanged();
   void lastPriceChanged();
   void bestPriceChanged();
   void sendFailed(const QString &reason);
   void sendingQuoteReply(const QString &reqId, double price);
   void pullingQuoteReply(const QString &reqId);
   void started();

private:
   BSQuoteRequest *quoteReq_;
   double   expirationInSec_;
   QString  security_;
   double   indicBid_, indicAsk_;
   double   lastPrice_ = 0;
   double   bestPrice_ = 0;
   std::shared_ptr<spdlog::logger> logger_;
   std::shared_ptr<AssetManager> assetManager_;
};


#endif // __USER_SCRIPT_H__
