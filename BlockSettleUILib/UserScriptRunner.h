/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/

#ifndef USERSCRIPTRUNNER_H_INCLUDED
#define USERSCRIPTRUNNER_H_INCLUDED

#include <QObject>
#include <QTimer>

#include <unordered_map>
#include <memory>
#include <string>
#include <mutex>

#include "UserScript.h"
#include "QuoteProvider.h"

QT_BEGIN_NAMESPACE
class QThread;
QT_END_NAMESPACE

namespace bs {
   namespace sync {
      class WalletsManager;
   }
}
class MDCallbacksQt;
class SignContainer;
class UserScriptRunner;


//
// UserScriptHandler
//

//! Handler of events in user script.
class UserScriptHandler : public QObject
{
   Q_OBJECT
public:
   explicit UserScriptHandler(const std::shared_ptr<spdlog::logger> &);
   ~UserScriptHandler() noexcept override;

   virtual void setWalletsManager(const std::shared_ptr<bs::sync::WalletsManager> &);
   void setParent(UserScriptRunner *);

signals:
   void scriptLoaded(const QString &fileName);
   void failedToLoad(const QString &fileName, const QString &error);

public slots:
   virtual void onThreadStopped()
   {
      deleteLater();
   }

protected slots:
   virtual void init(const QString &fileName) {}
   virtual void deinit() {}

protected:
   std::shared_ptr<spdlog::logger> logger_;
   std::shared_ptr<bs::sync::WalletsManager> walletsManager_;
}; // class UserScriptHandler


class AQScriptHandler : public UserScriptHandler
{
   Q_OBJECT
public:
   explicit AQScriptHandler(const std::shared_ptr<QuoteProvider> &
      , const std::shared_ptr<SignContainer> &
      , const std::shared_ptr<MDCallbacksQt> &
      , const std::shared_ptr<AssetManager> &
      , const std::shared_ptr<spdlog::logger> &);
   ~AQScriptHandler() noexcept override;

   void setWalletsManager(const std::shared_ptr<bs::sync::WalletsManager> &) override;

signals:
   void pullQuoteNotif(const std::string& settlementId, const std::string& reqId, const std::string& reqSessToken);
   void sendQuote(const bs::network::QuoteReqNotification &qrn, double price);

public slots:
   void onThreadStopped() override;

protected slots:
   void init(const QString &fileName) override;
   void deinit() override;

private slots:
   void onQuoteReqNotification(const bs::network::QuoteReqNotification &qrn);
   void onQuoteReqCancelled(const QString &reqId, bool userCancelled);
   void onQuoteNotifCancelled(const QString &reqId);
   void onQuoteReqRejected(const QString &reqId, const QString &);
   void onMDUpdate(bs::network::Asset::Type, const QString &security,
      bs::network::MDFields mdFields);
   void onBestQuotePrice(const QString reqId, double price, bool own);
   void onAQReply(const QString &reqId, double price);
   void onAQPull(const QString &reqId);
   void aqTick();

private:
   void clear();

private:
   AutoQuoter *aq_ = nullptr;
   std::shared_ptr<SignContainer>            signingContainer_;
   std::shared_ptr<MDCallbacksQt>            mdCallbacks_;
   std::shared_ptr<AssetManager> assetManager_;

   std::unordered_map<std::string, QObject*> aqObjs_;
   std::unordered_map<std::string, bs::network::QuoteReqNotification> aqQuoteReqs_;
   std::unordered_map<std::string, double>   bestQPrices_;

   struct MDInfo {
      double   bidPrice;
      double   askPrice;
      double   lastPrice;
   };
   std::unordered_map<std::string, MDInfo>  mdInfo_;

   bool aqEnabled_;
   QTimer *aqTimer_;
}; // class UserScriptHandler


class RFQScriptHandler : public UserScriptHandler
{
   Q_OBJECT
public:
   explicit RFQScriptHandler(const std::shared_ptr<spdlog::logger> &
      , const std::shared_ptr<MDCallbacksQt> &
      , UserScriptRunner *runner);
   ~RFQScriptHandler() noexcept override;

   void setWalletsManager(const std::shared_ptr<bs::sync::WalletsManager> &) override;

   void submitRFQ(const std::string &id, const QString &symbol, double amount
      , bool buy);
   void rfqAccepted(const std::string &id);
   void rfqCancelled(const std::string &id);
   void rfqExpired(const std::string &id);

signals:
   void sendRFQ(const std::string &id);
   void cancelRFQ(const std::string &id);

protected slots:
   void init(const QString &fileName) override;
   void deinit() override;

private slots:
   void onMDUpdate(bs::network::Asset::Type, const QString &security,
      bs::network::MDFields mdFields);
   void onSendRFQ(const std::string &id, double amount, bool buy);
   void onCancelRFQ(const std::string &id);
   void onStopRFQ(const std::string &id);

private:
   void clear();

private:
   AutoRFQ *rfq_{ nullptr };
   std::shared_ptr<MDCallbacksQt>            mdCallbacks_;

   std::unordered_map<std::string, QObject*> rfqObjs_;

   struct MDInfo {
      double   bidPrice;
      double   askPrice;
      double   lastPrice;
   };
   std::unordered_map<std::string, MDInfo>  mdInfo_;
};


class UserScriptRunner : public QObject
{
   Q_OBJECT
public:
   UserScriptRunner(const std::shared_ptr<spdlog::logger> &
      , UserScriptHandler *, QObject *parent);
   ~UserScriptRunner() noexcept override;

   void setWalletsManager(const std::shared_ptr<bs::sync::WalletsManager> &);

signals:
   void init(const QString &fileName);
   void deinit();
   void stateChanged(bool enabled);
   void scriptLoaded(const QString &fileName);
   void failedToLoad(const QString &fileName, const QString &error);

public slots:
   void enable(const QString &fileName);
   void disable();

protected:
   QThread *thread_;
   UserScriptHandler *script_;
   std::shared_ptr<spdlog::logger> logger_;
}; // class UserScriptRunner

class AQScriptRunner : public UserScriptRunner
{
   Q_OBJECT
public:
   AQScriptRunner(const std::shared_ptr<QuoteProvider> &,
      const std::shared_ptr<SignContainer> &,
      const std::shared_ptr<MDCallbacksQt> &,
      const std::shared_ptr<AssetManager> &,
      const std::shared_ptr<spdlog::logger> &,
      QObject *parent);
   ~AQScriptRunner() noexcept override;

signals:
   void pullQuoteNotif(const std::string& settlementId, const std::string& reqId, const std::string& reqSessToken);
   void sendQuote(const bs::network::QuoteReqNotification &qrn, double price);
};

class RFQScriptRunner : public UserScriptRunner
{
   Q_OBJECT
public:
   RFQScriptRunner(const std::shared_ptr<MDCallbacksQt> &,
      const std::shared_ptr<spdlog::logger> &,
      QObject *parent);
   ~RFQScriptRunner() noexcept override;

   void submitRFQ(const std::string &id, const QString &symbol, double amount
      , bool buy);
   void rfqAccepted(const std::string &id);
   void rfqCancelled(const std::string &id);
   void rfqExpired(const std::string &id);

signals:
   void sendRFQ(const std::string &id);
   void cancelRFQ(const std::string &id);
};

#endif // USERSCRIPTRUNNER_H_INCLUDED
