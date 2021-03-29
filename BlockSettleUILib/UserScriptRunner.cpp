/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/

#include "UserScriptRunner.h"
#include <QJsonObject>
#include <QJsonDocument>
#include <QThread>
#include <QTimer>
#include <spdlog/spdlog.h>
#include "DataConnectionListener.h"
#include "MDCallbacksQt.h"
#include "SignContainer.h"
#include "UserScript.h"
#include "Wallets/SyncWalletsManager.h"


//
// UserScriptHandler
//

UserScriptHandler::UserScriptHandler(const std::shared_ptr<spdlog::logger> &logger)
   :logger_(logger)
{}

UserScriptHandler::~UserScriptHandler() noexcept = default;

void UserScriptHandler::setParent(UserScriptRunner *runner)
{
   QObject::setParent(nullptr);

   connect(runner, &UserScriptRunner::init, this, &UserScriptHandler::init,
      Qt::QueuedConnection);
   connect(runner, &UserScriptRunner::deinit, this, &UserScriptHandler::deinit,
      Qt::QueuedConnection);
}

void UserScriptHandler::setWalletsManager(const std::shared_ptr<bs::sync::WalletsManager> &walletsManager)
{
   walletsManager_ = walletsManager;
}


AQScriptHandler::AQScriptHandler(const std::shared_ptr<QuoteProvider> &quoteProvider
   , const std::shared_ptr<SignContainer> &signingContainer
   , const std::shared_ptr<MDCallbacksQt> &mdCallbacks
   , const std::shared_ptr<AssetManager> &assetManager
   , const std::shared_ptr<spdlog::logger> &logger)
   : UserScriptHandler(logger)
   , signingContainer_(signingContainer), mdCallbacks_(mdCallbacks)
   , assetManager_(assetManager)
   , aqEnabled_(false)
   , aqTimer_(new QTimer(this))
{
   connect(quoteProvider.get(), &QuoteProvider::quoteReqNotifReceived,
      this, &AQScriptHandler::onQuoteReqNotification, Qt::QueuedConnection);
   connect(quoteProvider.get(), &QuoteProvider::quoteNotifCancelled,
      this, &AQScriptHandler::onQuoteNotifCancelled, Qt::QueuedConnection);
   connect(quoteProvider.get(), &QuoteProvider::quoteCancelled,
      this, &AQScriptHandler::onQuoteReqCancelled, Qt::QueuedConnection);
   connect(quoteProvider.get(), &QuoteProvider::quoteRejected,
      this, &AQScriptHandler::onQuoteReqRejected, Qt::QueuedConnection);

   connect(mdCallbacks_.get(), &MDCallbacksQt::MDUpdate, this, &AQScriptHandler::onMDUpdate,
      Qt::QueuedConnection);
   connect(quoteProvider.get(), &QuoteProvider::bestQuotePrice,
      this, &AQScriptHandler::onBestQuotePrice, Qt::QueuedConnection);

   aqTimer_->setInterval(500);
   connect(aqTimer_, &QTimer::timeout, this, &AQScriptHandler::aqTick);
   aqTimer_->start();
}

AQScriptHandler::~AQScriptHandler() noexcept
{
   clear();
}

void AQScriptHandler::onThreadStopped()
{
   aqTimer_->stop();
   UserScriptHandler::onThreadStopped();
}

void AQScriptHandler::setWalletsManager(const std::shared_ptr<bs::sync::WalletsManager> &walletsManager)
{
   UserScriptHandler::setWalletsManager(walletsManager);

   if (aq_) {
      aq_->setWalletsManager(walletsManager);
   }
}

void AQScriptHandler::setExtConnections(const ExtConnections &conns)
{
   if (conns.empty()) {
      extConns_.clear();
   }
   else {
      extConns_ = conns;
   }
}

void AQScriptHandler::reload(const QString &filename)
{
   if (aq_) {
      aq_->load(filename);
   }
}

void AQScriptHandler::onQuoteReqNotification(const bs::network::QuoteReqNotification &qrn)
{
   const auto &itAQObj = aqObjs_.find(qrn.quoteRequestId);
   if ((qrn.status == bs::network::QuoteReqNotification::PendingAck) || (qrn.status == bs::network::QuoteReqNotification::Replied)) {
      aqQuoteReqs_[qrn.quoteRequestId] = qrn;
      if ((qrn.assetType != bs::network::Asset::SpotFX) && (!signingContainer_ || signingContainer_->isOffline())) {
         logger_->error("[AQScriptHandler::onQuoteReqNotification] can't handle"
            " non-FX quote without online signer");
         return;
      }
      if (aqEnabled_ && aq_ && (itAQObj == aqObjs_.end())) {
         QObject *obj = aq_->instantiate(qrn);
         aqObjs_[qrn.quoteRequestId] = obj;
         if (thread_) {
            obj->moveToThread(thread_);
         }

         const auto &mdIt = mdInfo_.find(qrn.security);
         auto reqReply = qobject_cast<BSQuoteReqReply *>(obj);
         if (!reqReply) {
            logger_->error("[AQScriptHandler::onQuoteReqNotification] invalid AQ object instantiated");
            return;
         }
         if (mdIt != mdInfo_.end()) {
            if (mdIt->second.bidPrice > 0) {
               reqReply->setIndicBid(mdIt->second.bidPrice);
            }
            if (mdIt->second.askPrice > 0) {
               reqReply->setIndicAsk(mdIt->second.askPrice);
            }
            if (mdIt->second.lastPrice > 0) {
               reqReply->setLastPrice(mdIt->second.lastPrice);
            }
         }
         reqReply->start();
      }
   }
   else if ((qrn.status == bs::network::QuoteReqNotification::Rejected)
      || (qrn.status == bs::network::QuoteReqNotification::TimedOut)) {
      if (itAQObj != aqObjs_.end()) {
         const auto replyObj = qobject_cast<BSQuoteReqReply *>(itAQObj->second);
         if (replyObj) {
            emit replyObj->cancelled();
         }
      }
      stop(qrn.quoteRequestId);
   }
}

void AQScriptHandler::stop(const std::string &quoteReqId)
{
   const auto &itAQObj = aqObjs_.find(quoteReqId);
   aqQuoteReqs_.erase(quoteReqId);
   if (itAQObj != aqObjs_.end()) {
      itAQObj->second->deleteLater();
      aqObjs_.erase(itAQObj);
      bestQPrices_.erase(quoteReqId);
   }
}

void AQScriptHandler::onQuoteReqCancelled(const QString &reqId, bool userCancelled)
{
   const auto itQR = aqQuoteReqs_.find(reqId.toStdString());
   if (itQR == aqQuoteReqs_.end()) {
      return;
   }
   auto qrn = itQR->second;

   qrn.status = (userCancelled ? bs::network::QuoteReqNotification::Withdrawn :
      bs::network::QuoteReqNotification::PendingAck);
   onQuoteReqNotification(qrn);
}

void AQScriptHandler::onQuoteNotifCancelled(const QString &reqId)
{
   onQuoteReqCancelled(reqId, true);
}

void AQScriptHandler::onQuoteReqRejected(const QString &reqId, const QString &)
{
   onQuoteReqCancelled(reqId, true);
}

void AQScriptHandler::init(const QString &fileName)
{
   if (fileName.isEmpty()) {
      return;
   }
   aqEnabled_ = false;

   aq_ = new AutoQuoter(logger_, assetManager_, mdCallbacks_, extConns_, this);
   if (walletsManager_) {
      aq_->setWalletsManager(walletsManager_);
   }
   connect(aq_, &AutoQuoter::loaded, [this, fileName] {
      emit scriptLoaded(fileName);
      aqEnabled_ = true;
   });
   connect(aq_, &AutoQuoter::failed, [this, fileName](const QString &err) {
      logger_->error("Script loading failed: {}", err.toStdString());

      aq_->deleteLater();
      aq_ = nullptr;

      emit failedToLoad(fileName, err);
   });
   connect(aq_, &AutoQuoter::sendingQuoteReply, this, &AQScriptHandler::onAQReply);
   connect(aq_, &AutoQuoter::pullingQuoteReply, this, &AQScriptHandler::onAQPull);

   aq_->load(fileName);
}

void AQScriptHandler::deinit()
{
   clear();

   if (aq_) {
      aq_->deleteLater();
      aq_ = nullptr;
   }
}

void AQScriptHandler::clear()
{
   if (!aq_) {
      return;
   }

   for (auto aqObj : aqObjs_) {
      aqObj.second->deleteLater();
   }
   aqObjs_.clear();
   aqEnabled_ = false;

   std::vector<std::string> requests;
   for (const auto &aq : aqQuoteReqs_) {
      switch (aq.second.status) {
      case bs::network::QuoteReqNotification::PendingAck:
      case bs::network::QuoteReqNotification::Replied:
         requests.push_back(aq.first);
         break;
      case bs::network::QuoteReqNotification::Withdrawn:
      case bs::network::QuoteReqNotification::Rejected:
      case bs::network::QuoteReqNotification::TimedOut:
         break;
      case bs::network::QuoteReqNotification::StatusUndefined:
         assert(false);
         break;
      }
   }

   for (const std::string &reqId : requests) {
      SPDLOG_LOGGER_INFO(logger_, "pull AQ request {}", reqId);
      onAQPull(QString::fromStdString(reqId));
   }
}

void AQScriptHandler::onMDUpdate(bs::network::Asset::Type, const QString &security,
   bs::network::MDFields mdFields)
{
   auto &mdInfo = mdInfo_[security.toStdString()];
   mdInfo.merge(bs::network::MDField::get(mdFields));

   for (auto aqObj : aqObjs_) {
      QString sec = aqObj.second->property("security").toString();
      if (sec.isEmpty() || (security != sec)) {
         continue;
      }

      auto *reqReply = qobject_cast<BSQuoteReqReply *>(aqObj.second);
      if (mdInfo.bidPrice > 0) {
         reqReply->setIndicBid(mdInfo.bidPrice);
      }
      if (mdInfo.askPrice > 0) {
         reqReply->setIndicAsk(mdInfo.askPrice);
      }
      if (mdInfo.lastPrice > 0) {
         reqReply->setLastPrice(mdInfo.lastPrice);
      }
   }
}

void AQScriptHandler::onBestQuotePrice(const QString reqId, double price, bool own)
{
   const auto itAQObj = aqObjs_.find(reqId.toStdString());
   if (itAQObj != aqObjs_.end()) {
      qobject_cast<BSQuoteReqReply *>(itAQObj->second)->setBestPrice(price, own);
   }
}

void AQScriptHandler::onAQReply(const QString &reqId, double price)
{
   const auto itQRN = aqQuoteReqs_.find(reqId.toStdString());
   if (itQRN == aqQuoteReqs_.end()) {
      logger_->warn("[UserScriptHandler::onAQReply] QuoteReqNotification with id = {} not found", reqId.toStdString());
      return;
   }

   emit sendQuote(itQRN->second, price);
}

void AQScriptHandler::onAQPull(const QString &reqId)
{
   const auto itQRN = aqQuoteReqs_.find(reqId.toStdString());
   if (itQRN == aqQuoteReqs_.end()) {
      logger_->warn("[UserScriptHandler::onAQPull] QuoteReqNotification with id = {} not found", reqId.toStdString());
      return;
   }
   emit pullQuoteNotif(itQRN->second.settlementId, itQRN->second.quoteRequestId, itQRN->second.sessionToken);
}

void AQScriptHandler::aqTick()
{
   if (aqObjs_.empty()) {
      return;
   }
   QStringList expiredEntries;
   const auto timeNow = QDateTime::currentDateTime();

   for (auto aqObj : aqObjs_) {
      BSQuoteRequest *qr = qobject_cast<BSQuoteReqReply *>(aqObj.second)->quoteReq();
      if (!qr)  continue;
      auto itQRN = aqQuoteReqs_.find(qr->requestId().toStdString());
      if (itQRN == aqQuoteReqs_.end())  continue;
      const auto& expTime = QDateTime::fromMSecsSinceEpoch(itQRN->second.expirationTime);
      const auto timeDiff = timeNow.msecsTo(expTime.addMSecs(itQRN->second.timeSkewMs));
      if (timeDiff <= 0) {
         expiredEntries << qr->requestId();
      }
      else {
         aqObj.second->setProperty("expirationInSec", timeDiff / 1000.0);
      }
   }
   for (const auto & expReqId : qAsConst(expiredEntries)) {
      onQuoteReqCancelled(expReqId, true);
   }
}

void AQScriptHandler::performOnReplyAndStop(const std::string &quoteReqId
   , const std::function<void(BSQuoteReqReply *)> &cb)
{
   const auto &itAQObj = aqObjs_.find(quoteReqId);
   if (itAQObj != aqObjs_.end()) {
      const auto replyObj = qobject_cast<BSQuoteReqReply *>(itAQObj->second);
      if (replyObj && cb) {
         cb(replyObj);
      }
   }
   QTimer::singleShot(1000, [this, quoteReqId] { stop(quoteReqId); });
}

void AQScriptHandler::cancelled(const std::string &quoteReqId)
{
   performOnReplyAndStop(quoteReqId, [](BSQuoteReqReply *replyObj) {
      emit replyObj->cancelled();
   });
}

void AQScriptHandler::settled(const std::string &quoteReqId)
{
   performOnReplyAndStop(quoteReqId, [](BSQuoteReqReply *replyObj) {
      emit replyObj->settled();
   });
}

void AQScriptHandler::extMsgReceived(const std::string &data)
{
   QJsonParseError jsonError;
   const auto &jsonDoc = QJsonDocument::fromJson(QByteArray::fromStdString(data)
      , &jsonError);
   if (jsonError.error != QJsonParseError::NoError) {
      logger_->error("[AQScriptHandler::extMsgReceived] invalid JSON message: {}"
         , jsonError.errorString().toUtf8().toStdString());
      return;
   }
   const auto &jsonObj = jsonDoc.object();
   const auto &strFrom = jsonObj[QLatin1Literal("from")].toString();
   const auto &strType = jsonObj[QLatin1Literal("type")].toString();
   const auto &msgObj = jsonObj[QLatin1Literal("message")].toObject();
   QJsonDocument msgDoc(msgObj);
   const auto &strMsg = QString::fromStdString(msgDoc.toJson(QJsonDocument::Compact).toStdString());
   if (strFrom.isEmpty() || strType.isEmpty() || msgObj.isEmpty()) {
      logger_->error("[AQScriptHandler::extMsgReceived] invalid data in JSON: {}"
         , data);
      return;
   }

   for (const auto &aqObj : aqObjs_) {
      const auto replyObj = qobject_cast<BSQuoteReqReply *>(aqObj.second);
      if (replyObj) {
         emit replyObj->extDataReceived(strFrom, strType, strMsg);
      }
   }
}


//
// UserScriptRunner
//
UserScriptRunner::UserScriptRunner(const std::shared_ptr<spdlog::logger> &logger
   , UserScriptHandler *script, QObject *parent)
   : QObject(parent)
   , thread_(new QThread(this)), script_(script), logger_(logger)
{
   script_->setParent(this);
   connect(thread_, &QThread::finished, script_, &UserScriptHandler::onThreadStopped
      , Qt::QueuedConnection);
   script_->setRunningThread(thread_);

   connect(script_, &UserScriptHandler::scriptLoaded, this, &UserScriptRunner::scriptLoaded);
   connect(script_, &UserScriptHandler::failedToLoad, this, &UserScriptRunner::failedToLoad);

   thread_->start();
}

UserScriptRunner::~UserScriptRunner() noexcept
{
   thread_->quit();
   thread_->wait();
}

void UserScriptRunner::setWalletsManager(const std::shared_ptr<bs::sync::WalletsManager> &walletsManager)
{
   script_->setWalletsManager(walletsManager);
}

void UserScriptRunner::enable(const QString &fileName)
{
   logger_->info("Load script {}", fileName.toStdString());
   emit init(fileName);
}

void UserScriptRunner::disable()
{
   logger_->info("Unload script");
   emit deinit();
}


AQScriptRunner::AQScriptRunner(const std::shared_ptr<QuoteProvider> &quoteProvider
   , const std::shared_ptr<SignContainer> &signingContainer
   , const std::shared_ptr<MDCallbacksQt> &mdCallbacks
   , const std::shared_ptr<AssetManager> &assetManager
   , const std::shared_ptr<spdlog::logger> &logger
   , QObject *parent)
   : UserScriptRunner(logger, new AQScriptHandler(quoteProvider, signingContainer,
      mdCallbacks, assetManager, logger), parent)
{
   thread_->setObjectName(QStringLiteral("AQScriptRunner"));

   connect((AQScriptHandler *)script_, &AQScriptHandler::pullQuoteNotif, this
      , &AQScriptRunner::pullQuoteNotif);
   connect((AQScriptHandler *)script_, &AQScriptHandler::sendQuote, this
      , &AQScriptRunner::sendQuote);
}

AQScriptRunner::~AQScriptRunner()
{
   const auto aqHandler = qobject_cast<AQScriptHandler *>(script_);
   if (aqHandler) {
      aqHandler->setExtConnections({});
   }
}

void AQScriptRunner::cancelled(const std::string &quoteReqId)
{
   const auto aqHandler = qobject_cast<AQScriptHandler *>(script_);
   if (aqHandler) {
      aqHandler->cancelled(quoteReqId);
   }
}

void AQScriptRunner::settled(const std::string &quoteReqId)
{
   const auto aqHandler = qobject_cast<AQScriptHandler *>(script_);
   if (aqHandler) {
      aqHandler->settled(quoteReqId);
   }
}

class ExtConnListener : public DataConnectionListener
{
public:
   ExtConnListener(AQScriptRunner *parent, std::shared_ptr<spdlog::logger> &logger)
      : parent_(parent), logger_(logger)
   {}

   void OnDataReceived(const std::string &data) override
   {
      parent_->onExtDataReceived(data);
   }

   void OnConnected() override { logger_->debug("[{}]", __func__); }
   void OnDisconnected() override { logger_->debug("[{}]", __func__); }
   void OnError(DataConnectionError err) override { logger_->debug("[{}] {}", __func__, (int)err); }

private:
   AQScriptRunner *parent_{nullptr};
   std::shared_ptr<spdlog::logger>  logger_;
};

void AQScriptRunner::setExtConnections(const ExtConnections &conns)
{
   const auto aqHandler = qobject_cast<AQScriptHandler *>(script_);
   if (aqHandler) {
      aqHandler->setExtConnections(conns);
   }
}

std::shared_ptr<DataConnectionListener> AQScriptRunner::getExtConnListener()
{
   if (!extConnListener_) {
      extConnListener_ = std::make_shared<ExtConnListener>(this, logger_);
   }
   return extConnListener_;
}

void AQScriptRunner::onExtDataReceived(const std::string &data)
{
   const auto aqHandler = qobject_cast<AQScriptHandler *>(script_);
   if (aqHandler) {
      aqHandler->extMsgReceived(data);
   }
}


RFQScriptHandler::RFQScriptHandler(const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<MDCallbacksQt> &mdCallbacks)
   : UserScriptHandler(logger)
   , mdCallbacks_(mdCallbacks)
{
   connect(mdCallbacks_.get(), &MDCallbacksQt::MDUpdate, this, &RFQScriptHandler::onMDUpdate,
      Qt::QueuedConnection);
}

RFQScriptHandler::~RFQScriptHandler() noexcept
{
   clear();
}


void RFQScriptHandler::setWalletsManager(const std::shared_ptr<bs::sync::WalletsManager> &walletsManager)
{
   UserScriptHandler::setWalletsManager(walletsManager);

   if (rfq_) {
      rfq_->setWalletsManager(walletsManager);
   }
}

void RFQScriptHandler::reload(const QString &filename)
{
   if (rfq_) {
      rfq_->load(filename);
   }
}

void RFQScriptHandler::init(const QString &fileName)
{
   if (fileName.isEmpty()) {
      emit failedToLoad(fileName, tr("invalid script filename"));
      return;
   }
   if (rfq_) {
      start();
      return;  // already inited
   }

   rfq_ = new AutoRFQ(logger_, mdCallbacks_, this);
   if (walletsManager_) {
      rfq_->setWalletsManager(walletsManager_);
   }
   connect(rfq_, &AutoQuoter::loaded, [this, fileName] {
      emit scriptLoaded(fileName);
   });
   connect(rfq_, &AutoQuoter::failed, [this, fileName](const QString &err) {
      logger_->error("Script loading failed: {}", err.toStdString());
      if (rfq_) {
         rfq_->deleteLater();
         rfq_ = nullptr;
      }
      emit failedToLoad(fileName, err);
   });

   connect(rfq_, &AutoRFQ::sendRFQ, this, &RFQScriptHandler::sendRFQ);
   connect(rfq_, &AutoRFQ::cancelRFQ, this, &RFQScriptHandler::cancelRFQ);
   connect(rfq_, &AutoRFQ::stopRFQ, this, &RFQScriptHandler::onStopRFQ);

   if (!rfq_->load(fileName)) {
      emit failedToLoad(fileName, tr("script loading failed"));
   }
   start();
}

void RFQScriptHandler::deinit()
{
   clear();

   if (rfq_) {
      rfq_->deleteLater();
      rfq_ = nullptr;
   }
}

void RFQScriptHandler::clear()
{
   if (!rfq_) {
      return;
   }
   if (rfqObj_) {
      rfqObj_->cancelAll();
      rfqObj_->deleteLater();
      rfqObj_ = nullptr;
   }
}

void RFQScriptHandler::onMDUpdate(bs::network::Asset::Type at, const QString &security,
   bs::network::MDFields mdFields)
{
   if (!rfqObj_) {
      return;
   }
   rfqObj_->onMDUpdate(at, security, mdFields);
}

void RFQScriptHandler::start()
{
   if (!rfq_) {
      return;
   }
   if (!rfqObj_) {
      auto obj = rfq_->instantiate();
      if (!obj) {
         emit failedToLoad({}, tr("failed to instantiate"));
         return;
      }
      rfqObj_ = qobject_cast<RFQScript *>(obj);
      if (!rfqObj_) {
         logger_->error("[RFQScriptHandler::start] {} has wrong script object");
         emit failedToLoad({}, tr("wrong script loaded"));
         return;
      }
   }
   logger_->debug("[RFQScriptHandler::start]");
   rfqObj_->moveToThread(thread_);
   rfqObj_->start();
}

void RFQScriptHandler::suspend()
{
   if (rfqObj_) {
      rfqObj_->suspend();
   }
}

void RFQScriptHandler::rfqAccepted(const std::string &id)
{
   if (rfqObj_) {
      rfqObj_->onAccepted(id);
   }
}

void RFQScriptHandler::rfqCancelled(const std::string &id)
{
   if (rfqObj_) {
      rfqObj_->onCancelled(id);
   }
}

void RFQScriptHandler::rfqExpired(const std::string &id)
{
   if (rfqObj_) {
      rfqObj_->onExpired(id);
   }
}

void RFQScriptHandler::onStopRFQ(const std::string &id)
{
   if (rfqObj_) {
      rfqObj_->onCancelled(id);
   }
}


RFQScriptRunner::RFQScriptRunner(const std::shared_ptr<MDCallbacksQt> &mdCallbacks
   , const std::shared_ptr<spdlog::logger> &logger
   , QObject *parent)
   : UserScriptRunner(logger, new RFQScriptHandler(logger, mdCallbacks), parent)
{
   thread_->setObjectName(QStringLiteral("RFQScriptRunner"));

   connect((RFQScriptHandler *)script_, &RFQScriptHandler::sendRFQ, this
      , &RFQScriptRunner::sendRFQ);
   connect((RFQScriptHandler *)script_, &RFQScriptHandler::cancelRFQ, this
      , &RFQScriptRunner::cancelRFQ);
}

RFQScriptRunner::~RFQScriptRunner() = default;

void RFQScriptRunner::start(const QString &filename)
{
   emit init(filename);
}

void RFQScriptRunner::suspend()
{
   ((RFQScriptHandler *)script_)->suspend();
}

void RFQScriptRunner::rfqAccepted(const std::string &id)
{
   ((RFQScriptHandler *)script_)->rfqAccepted(id);
}

void RFQScriptRunner::rfqCancelled(const std::string &id)
{
   ((RFQScriptHandler *)script_)->rfqCancelled(id);
}

void RFQScriptRunner::rfqExpired(const std::string &id)
{
   ((RFQScriptHandler *)script_)->rfqExpired(id);
}
