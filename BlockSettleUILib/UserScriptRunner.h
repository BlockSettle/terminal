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

#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include "UserScript.h"
#include "QuoteProvider.h"
#include "CommonTypes.h"

QT_BEGIN_NAMESPACE
class QThread;
QT_END_NAMESPACE

namespace bs {
   namespace sync {
      class WalletsManager;
   }
}
class DataConnectionListener;
class MDCallbacksQt;
class RFQScript;
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
   void setRunningThread(QThread *thread) { thread_ = thread; }
   virtual void reload(const QString &filename) = 0;

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
   QThread *thread_{nullptr};
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
   void setExtConnections(const ExtConnections &conns);
   void reload(const QString &filename) override;

   void cancelled(const std::string &quoteReqId);
   void settled(const std::string &quoteReqId);
   void extMsgReceived(const std::string &data);

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
   void stop(const std::string &quoteReqId);
   void performOnReplyAndStop(const std::string &quoteReqId
      , const std::function<void(BSQuoteReqReply *)> &);

private:
   AutoQuoter *aq_ = nullptr;
   std::shared_ptr<SignContainer>            signingContainer_;
   std::shared_ptr<MDCallbacksQt>            mdCallbacks_;
   std::shared_ptr<AssetManager> assetManager_;
   ExtConnections                extConns_;

   std::unordered_map<std::string, QObject*> aqObjs_;
   std::unordered_map<std::string, bs::network::QuoteReqNotification> aqQuoteReqs_;
   std::unordered_map<std::string, double>   bestQPrices_;

   std::unordered_map<std::string, bs::network::MDInfo>  mdInfo_;

   struct ExtMessage {
      QString  from;
      QString  type;
      QString  msg;
   };
   std::deque<ExtMessage>  extDataPool_;
   const size_t maxExtDataPoolSize_{ 100 };
   std::mutex  mtxExtData_;

   bool aqEnabled_;
   QTimer *aqTimer_;
}; // class UserScriptHandler


class RFQScriptHandler : public UserScriptHandler
{
   Q_OBJECT
public:
   explicit RFQScriptHandler(const std::shared_ptr<spdlog::logger> &
      , const std::shared_ptr<MDCallbacksQt> &);
   ~RFQScriptHandler() noexcept override;

   void setWalletsManager(const std::shared_ptr<bs::sync::WalletsManager> &) override;
   void reload(const QString &filename) override;

   void suspend();
   void rfqAccepted(const std::string &id);
   void rfqCancelled(const std::string &id);
   void rfqExpired(const std::string &id);

signals:
   void sendRFQ(const std::string &id, const QString &symbol, double amount, bool buy);
   void cancelRFQ(const std::string &id);

protected slots:
   void init(const QString &fileName) override;
   void deinit() override;

private slots:
   void onMDUpdate(bs::network::Asset::Type, const QString &security,
      bs::network::MDFields mdFields);
   void onStopRFQ(const std::string &id);

private:
   void clear();
   void start();

private:
   AutoRFQ *rfq_{ nullptr };
   std::shared_ptr<MDCallbacksQt>            mdCallbacks_;
   RFQScript * rfqObj_{ nullptr };
};


class UserScriptRunner : public QObject
{
   Q_OBJECT
public:
   UserScriptRunner(const std::shared_ptr<spdlog::logger> &
      , UserScriptHandler *, QObject *parent);
   ~UserScriptRunner() noexcept override;

   void setWalletsManager(const std::shared_ptr<bs::sync::WalletsManager> &);
   void setRunningThread(QThread *thread) { script_->setRunningThread(thread); }
   void reload(const QString &filename) { script_->reload(filename); }

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
   AQScriptRunner(const std::shared_ptr<QuoteProvider> &
      , const std::shared_ptr<SignContainer> &
      , const std::shared_ptr<MDCallbacksQt> &
      , const std::shared_ptr<AssetManager> &
      , const std::shared_ptr<spdlog::logger> &, QObject *parent = nullptr);
   ~AQScriptRunner() noexcept override;

   void cancelled(const std::string &quoteReqId);
   void settled(const std::string &quoteReqId);

   void setExtConnections(const ExtConnections &);
   std::shared_ptr<DataConnectionListener> getExtConnListener();
   void onExtDataReceived(const std::string &data);

signals:
   void pullQuoteNotif(const std::string& settlementId, const std::string& reqId, const std::string& reqSessToken);
   void sendQuote(const bs::network::QuoteReqNotification &qrn, double price);

private:
   std::shared_ptr<DataConnectionListener>   extConnListener_;
};

class RFQScriptRunner : public UserScriptRunner
{
   Q_OBJECT
public:
   RFQScriptRunner(const std::shared_ptr<MDCallbacksQt> &,
      const std::shared_ptr<spdlog::logger> &,
      QObject *parent);
   ~RFQScriptRunner() noexcept override;

   void start(const QString &file);
   void suspend();
   void rfqAccepted(const std::string &id);
   void rfqCancelled(const std::string &id);
   void rfqExpired(const std::string &id);

signals:
   void sendRFQ(const std::string &id, const QString &symbol, double amount, bool buy);
   void cancelRFQ(const std::string &id);
};

#endif // USERSCRIPTRUNNER_H_INCLUDED
