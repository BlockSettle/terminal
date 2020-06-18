/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
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
class MDCallbacksQt;


//
// MarketData
//

//! Market data for user script.
class MarketData : public QObject
{
   Q_OBJECT

public:
   MarketData(const std::shared_ptr<MDCallbacksQt> &, QObject *parent);
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
   UserScript(const std::shared_ptr<spdlog::logger> &,
      const std::shared_ptr<MDCallbacksQt> &,
      QObject* parent = nullptr);
   ~UserScript() override;

   void setWalletsManager(std::shared_ptr<bs::sync::WalletsManager> walletsManager);
   bool load(const QString &filename);

signals:
   void loaded();
   void failed(const QString &desc);

protected:
   QObject *instantiate();

protected:
   std::shared_ptr<spdlog::logger> logger_;
   QQmlEngine *engine_;
   QQmlComponent *component_;
   MarketData *md_;
   Constants *const_;
   DataStorage *storage_;
};


class AutoQuoter : public UserScript
{
   Q_OBJECT
public:
   AutoQuoter(const std::shared_ptr<spdlog::logger> &
      , const std::shared_ptr<AssetManager> &
      , const std::shared_ptr<MDCallbacksQt> &
      , QObject* parent = nullptr);
   ~AutoQuoter() override = default;

   QObject *instantiate(const bs::network::QuoteReqNotification &qrn);

signals:
   void sendingQuoteReply(const QString &reqId, double price);
   void pullingQuoteReply(const QString &reqId);

private:
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
   Q_PROPERTY(double lastPrice READ lastPrice NOTIFY lastPriceChanged)
   Q_PROPERTY(double bestPrice READ bestPrice NOTIFY bestPriceChanged)
   Q_PROPERTY(double isOwnBestPrice READ isOwnBestPrice)

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

         if (started_) {
            emit indicBidChanged();
         }
      }
   }
   void setIndicAsk(double prc) {
      if (indicAsk_ != prc) {
         indicAsk_ = prc;

         if (started_) {
            emit indicAskChanged();
         }
      }
   }
   void setLastPrice(double prc) {
      if (lastPrice_ != prc) {
         lastPrice_ = prc;

         if (started_) {
            emit lastPriceChanged();
         }
      }
   }
   void setBestPrice(double prc, bool own) {
      isOwnBestPrice_ = own;
      if (bestPrice_ != prc) {
         bestPrice_ = prc;
         emit bestPriceChanged();
      }
   }
   double indicBid() const { return indicBid_; }
   double indicAsk() const { return indicAsk_; }
   double lastPrice() const { return lastPrice_; }
   double bestPrice() const { return bestPrice_; }
   bool   isOwnBestPrice() const { return isOwnBestPrice_; }

   void init(const std::shared_ptr<spdlog::logger> &logger, const std::shared_ptr<AssetManager> &assetManager);

   Q_INVOKABLE void log(const QString &);
   Q_INVOKABLE bool sendQuoteReply(double price);
   Q_INVOKABLE bool pullQuoteReply();
   Q_INVOKABLE QString product();
   Q_INVOKABLE double accountBalance(const QString &product);

   void start() {
      if (!started_ && indicBid_ > .0 && indicAsk_ > .0 && lastPrice_ > .0) {
         started_ = true;
         emit started();
      }
   }

signals:
   void expirationInSecChanged();
   void indicBidChanged();
   void indicAskChanged();
   void lastPriceChanged();
   void bestPriceChanged();
   void sendingQuoteReply(const QString &reqId, double price);
   void pullingQuoteReply(const QString &reqId);
   void started();

private:
   BSQuoteRequest *quoteReq_;
   double   expirationInSec_;
   QString  security_;
   double   indicBid_ = 0;
   double   indicAsk_ = 0;
   double   lastPrice_ = 0;
   double   bestPrice_ = 0;
   bool     isOwnBestPrice_ = false;
   bool     started_ = false;
   std::shared_ptr<spdlog::logger> logger_;
   std::shared_ptr<AssetManager> assetManager_;
};


class SubmitRFQ : public QObject
{  // Container for individual RFQ submitted
   Q_OBJECT
   Q_PROPERTY(QString id READ id)
   Q_PROPERTY(QString security READ security)
   Q_PROPERTY(double amount READ amount WRITE setAmount NOTIFY amountChanged)
   Q_PROPERTY(bool buy READ buy NOTIFY buyChanged)
   Q_PROPERTY(double indicBid READ indicBid WRITE setIndicBid NOTIFY indicBidChanged)
   Q_PROPERTY(double indicAsk READ indicAsk WRITE setIndicAsk NOTIFY indicAskChanged)
   Q_PROPERTY(double lastPrice READ lastPrice WRITE setLastPrice NOTIFY lastPriceChanged)

public:
   explicit SubmitRFQ(QObject *parent = nullptr) : QObject(parent) {}
   ~SubmitRFQ() override = default;

   void setId(const std::string &id) { id_ = QString::fromStdString(id); }
   QString id() const { return id_; }

   void setSecurity(const QString &security) { security_ = security; }
   QString security() const { return security_; }

   void setAmount(double amount)
   {
      if (amount_ != amount) {
         amount_ = amount;
         emit amountChanged();
      }
   }
   double amount() const { return amount_; }

   void setBuy(bool buy)
   {
      buy_ = buy;
      emit buyChanged();
   }
   bool buy() const { return buy_; }

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
   double indicBid() const { return indicBid_; }
   double indicAsk() const { return indicAsk_; }
   double lastPrice() const { return lastPrice_; }

   Q_INVOKABLE void stop();

signals:
   void amountChanged();
   void buyChanged();
   void indicBidChanged();
   void indicAskChanged();
   void lastPriceChanged();
   void stopRFQ(const std::string &id);
   void cancelled();
   void accepted();
   void expired();

private:
   QString  id_;
   QString  security_;
   double   amount_ = 0;
   bool     buy_ = true;
   double   indicBid_ = 0;
   double   indicAsk_ = 0;
   double   lastPrice_ = 0;
   std::shared_ptr<AssetManager> assetManager_;
};
Q_DECLARE_METATYPE(SubmitRFQ *)


class RFQScript : public QObject
{     // Main QML-script facing class
   Q_OBJECT

public:
   explicit RFQScript(QObject *parent = nullptr) : QObject(parent) {}
   ~RFQScript() override = default;

   void init(const std::shared_ptr<spdlog::logger> &logger)
   {
      logger_ = logger;
   }

   void start() {
      if (!started_) {
         started_ = true;
         emit started();
      }
   }

   void suspend() {
      if (started_) {
         started_ = false;
         emit suspended();
      }
   }

   Q_INVOKABLE void log(const QString &);
   Q_INVOKABLE SubmitRFQ *sendRFQ(const QString &symbol, bool buy, double amount);
   Q_INVOKABLE void cancelRFQ(const std::string &id);
   Q_INVOKABLE SubmitRFQ *activeRFQ(const QString &id);
   void cancelAll();

   void onMDUpdate(bs::network::Asset::Type, const QString &security,
      bs::network::MDFields mdFields);

   void onAccepted(const std::string &id);
   void onExpired(const std::string &id);
   void onCancelled(const std::string &id);

signals:
   void started();
   void suspended();
   void sendingRFQ(SubmitRFQ *);
   void cancellingRFQ(const std::string &id);

   void indicBidChanged(const QString &security, double price);
   void indicAskChanged(const QString &security, double price);
   void lastPriceChanged(const QString &security, double price);

   void cancelled(const QString &id);
   void accepted(const QString &id);
   void expired(const QString &id);

private:
   bool  started_ = false;
   std::shared_ptr<spdlog::logger> logger_;
   std::unordered_map<std::string, SubmitRFQ *> activeRFQs_;

   struct MDInfo {
      double   bidPrice;
      double   askPrice;
      double   lastPrice;
   };
   std::unordered_map<std::string, MDInfo>  mdInfo_;
};


class AutoRFQ : public UserScript
{
   Q_OBJECT
public:
   AutoRFQ(const std::shared_ptr<spdlog::logger> &
      , const std::shared_ptr<MDCallbacksQt> &
      , QObject* parent = nullptr);
   ~AutoRFQ() override = default;

   QObject *instantiate();

private slots:
   void onSendRFQ(SubmitRFQ *);

signals:
   void loaded();
   void failed(const QString &desc);
   void sendRFQ(const std::string &id, const QString &symbol, double amount, bool buy);
   void cancelRFQ(const std::string &id);
   void stopRFQ(const std::string &id);
};


#endif // __USER_SCRIPT_H__
