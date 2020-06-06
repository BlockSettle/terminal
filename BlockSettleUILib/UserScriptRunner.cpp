/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/

#include "UserScriptRunner.h"
#include "SignContainer.h"
#include "MDCallbacksQt.h"
#include "UserScript.h"
#include "Wallets/SyncWalletsManager.h"

#include <QThread>
#include <QTimer>


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

void AQScriptHandler::onQuoteReqNotification(const bs::network::QuoteReqNotification &qrn)
{
   const auto itAQObj = aqObjs_.find(qrn.quoteRequestId);
   if ((qrn.status == bs::network::QuoteReqNotification::PendingAck) || (qrn.status == bs::network::QuoteReqNotification::Replied)) {
      aqQuoteReqs_[qrn.quoteRequestId] = qrn;
      if ((qrn.assetType != bs::network::Asset::SpotFX) && (!signingContainer_ || signingContainer_->isOffline())) {
         return;
      }
      if (aqEnabled_ && aq_ && (itAQObj == aqObjs_.end())) {
         QObject *obj = aq_->instantiate(qrn);
         aqObjs_[qrn.quoteRequestId] = obj;

         const auto &mdIt = mdInfo_.find(qrn.security);
         if (mdIt != mdInfo_.end()) {
            auto *reqReply = qobject_cast<BSQuoteReqReply *>(obj);
            if (mdIt->second.bidPrice > 0) {
               reqReply->setIndicBid(mdIt->second.bidPrice);
            }
            if (mdIt->second.askPrice > 0) {
               reqReply->setIndicAsk(mdIt->second.askPrice);
            }
            if (mdIt->second.lastPrice > 0) {
               reqReply->setLastPrice(mdIt->second.lastPrice);
            }
            reqReply->start();
         }
      }
   }
   else {
      aqQuoteReqs_.erase(qrn.quoteRequestId);
      if (itAQObj != aqObjs_.end()) {
         itAQObj->second->deleteLater();
         aqObjs_.erase(qrn.quoteRequestId);
         bestQPrices_.erase(qrn.quoteRequestId);
      }
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

   aq_ = new AutoQuoter(logger_, fileName, assetManager_, mdCallbacks_, this);
   if (walletsManager_) {
      aq_->setWalletsManager(walletsManager_);
   }
   connect(aq_, &AutoQuoter::loaded, [this, fileName] {
      emit scriptLoaded(fileName);
      aqEnabled_ = true;
   });
   connect(aq_, &AutoQuoter::failed, [this, fileName](const QString &err) {
      logger_->error("Script loading failed: {}", err.toStdString());

      if (aq_) {
         aq_->deleteLater();
         aq_ = nullptr;
      }

      emit failedToLoad(fileName, err);
   });
   connect(aq_, &AutoQuoter::sendingQuoteReply, this, &AQScriptHandler::onAQReply);
   connect(aq_, &AutoQuoter::pullingQuoteReply, this, &AQScriptHandler::onAQPull);
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
   const double bid = bs::network::MDField::get(mdFields, bs::network::MDField::PriceBid).value;
   const double ask = bs::network::MDField::get(mdFields, bs::network::MDField::PriceOffer).value;
   const double last = bs::network::MDField::get(mdFields, bs::network::MDField::PriceLast).value;

   auto &mdInfo = mdInfo_[security.toStdString()];
   if (bid > 0) {
      mdInfo.bidPrice = bid;
   }
   if (ask > 0) {
      mdInfo.askPrice = ask;
   }
   if (last > 0) {
      mdInfo.lastPrice = last;
   }

   for (auto aqObj : aqObjs_) {
      QString sec = aqObj.second->property("security").toString();
      if (sec.isEmpty() || (security != sec)) {
         continue;
      }

      auto *reqReply = qobject_cast<BSQuoteReqReply *>(aqObj.second);
      if (bid > 0) {
         reqReply->setIndicBid(bid);
      }
      if (ask > 0) {
         reqReply->setIndicAsk(ask);
      }
      if (last > 0) {
         reqReply->setLastPrice(last);
      }
      reqReply->start();
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
      const auto timeDiff = timeNow.msecsTo(itQRN->second.expirationTime.addMSecs(itQRN->second.timeSkewMs));
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

//
// UserScriptRunner
//

UserScriptRunner::UserScriptRunner(const std::shared_ptr<spdlog::logger> &logger
   , UserScriptHandler *script, QObject *parent)
   : QObject(parent)
   , thread_(new QThread(this)), script_(script), logger_(logger)
{
   script_->setParent(this);
//   script_->moveToThread(thread_);   //FIXME: QmlEngine can't run in different QThread
   connect(thread_, &QThread::finished, script_, &UserScriptHandler::onThreadStopped
      , Qt::QueuedConnection);

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

AQScriptRunner::~AQScriptRunner() = default;


RFQScriptHandler::RFQScriptHandler(const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<MDCallbacksQt> &mdCallbacks
   , UserScriptRunner *runner)
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

void RFQScriptHandler::init(const QString &fileName)
{
   if (fileName.isEmpty()) {
      return;
   }

   rfq_ = new AutoRFQ(logger_, fileName, mdCallbacks_, this);
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

   connect(rfq_, &AutoRFQ::sendRFQ, this, &RFQScriptHandler::onSendRFQ);
   connect(rfq_, &AutoRFQ::cancelRFQ, this, &RFQScriptHandler::onCancelRFQ);
   connect(rfq_, &AutoRFQ::stopRFQ, this, &RFQScriptHandler::onStopRFQ);
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

   for (auto rfq : rfqObjs_) {
      onCancelRFQ(rfq.first);
      rfq.second->deleteLater();
   }
   rfqObjs_.clear();
}

void RFQScriptHandler::onMDUpdate(bs::network::Asset::Type, const QString &security,
   bs::network::MDFields mdFields)
{
   const double bid = bs::network::MDField::get(mdFields, bs::network::MDField::PriceBid).value;
   const double ask = bs::network::MDField::get(mdFields, bs::network::MDField::PriceOffer).value;
   const double last = bs::network::MDField::get(mdFields, bs::network::MDField::PriceLast).value;

   auto &mdInfo = mdInfo_[security.toStdString()];
   if (bid > 0) {
      mdInfo.bidPrice = bid;
   }
   if (ask > 0) {
      mdInfo.askPrice = ask;
   }
   if (last > 0) {
      mdInfo.lastPrice = last;
   }

   for (auto rfq : rfqObjs_) {
      const auto &sec = rfq.second->property("security").toString();
      if (sec.isEmpty() || (security != sec)) {
         continue;
      }
      if (bid > 0) {
         rfq.second->setProperty("indicBid", bid);
      }
      if (ask > 0) {
         rfq.second->setProperty("indicAsk", ask);
      }
      if (last > 0) {
         rfq.second->setProperty("lastPrice", last);
      }
   }
}

void RFQScriptHandler::submitRFQ(const std::string &id, const QString &symbol
   , double amount, bool buy)
{
   if (!rfq_) {
      return;
   }
   QObject *obj = nullptr;
   const auto &itRFQ = rfqObjs_.find(id);
   if (itRFQ == rfqObjs_.end()) {
      obj = rfq_->instantiate(id, symbol, amount, buy);
      rfqObjs_[id] = obj;
      auto rfqObj = qobject_cast<SubmitRFQ *>(obj);
      if (!rfqObj) {
         logger_->error("[RFQScriptHandler::submitRFQ] {} has wrong script object", id);
         emit failedToLoad({}, tr("wrong script for %1").arg(QString::fromStdString(id)));
         return;
      }
      logger_->debug("[RFQScriptHandler::submitRFQ] {}", id);
      rfqObj->start();
   }
   else {
      logger_->debug("[RFQScriptHandler::submitRFQ] {} already exists", id);
      obj = itRFQ->second;
   }

   if (!obj) {
      emit failedToLoad({}, tr("failed to instantiate %1").arg(QString::fromStdString(id)));
      return;
   }

   auto *reqReply = qobject_cast<SubmitRFQ *>(obj);
   const auto &mdIt = mdInfo_.find(symbol.toStdString());
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

void RFQScriptHandler::rfqAccepted(const std::string &id)
{
   const auto &itRFQ = rfqObjs_.find(id);
   if (itRFQ == rfqObjs_.end()) {
      return;
   }
   const auto rfq = qobject_cast<SubmitRFQ *>(itRFQ->second);
   if (rfq) {
      emit rfq->accepted();
      rfq->deleteLater();
   }
   rfqObjs_.erase(itRFQ);
}

void RFQScriptHandler::rfqCancelled(const std::string &id)
{
   const auto &itRFQ = rfqObjs_.find(id);
   if (itRFQ == rfqObjs_.end()) {
      logger_->warn("[RFQScriptHandler::rfqCancelled] failed to find id {}", id);
      return;
   }
   const auto rfq = qobject_cast<SubmitRFQ *>(itRFQ->second);
   if (rfq) {
      emit rfq->cancelled();
      rfq->deleteLater();
   }
   rfqObjs_.erase(itRFQ);
}

void RFQScriptHandler::rfqExpired(const std::string &id)
{
   const auto &itRFQ = rfqObjs_.find(id);
   if (itRFQ == rfqObjs_.end()) {
      logger_->warn("[RFQScriptHandler::rfqExpired] failed to find id {}", id);
      return;
   }
   const auto rfq = qobject_cast<SubmitRFQ *>(itRFQ->second);
   if (rfq) {
      emit rfq->expired();
   }
}

void RFQScriptHandler::onSendRFQ(const std::string &id, double amount, bool buy)
{
   if (rfqObjs_.find(id) == rfqObjs_.end()) {
      logger_->warn("[RFQScriptHandler::onSendRFQ] id {} not found", id);
      return;
   }
   emit sendRFQ(id);
}

void RFQScriptHandler::onCancelRFQ(const std::string &id)
{
   if (rfqObjs_.find(id) == rfqObjs_.end()) {
      logger_->warn("[RFQScriptHandler::onCancelRFQ] id {} not found", id);
      return;
   }
   emit cancelRFQ(id);
}

void RFQScriptHandler::onStopRFQ(const std::string &id)
{
   const auto &itRFQ = rfqObjs_.find(id);
   if (itRFQ == rfqObjs_.end()) {
      logger_->warn("[RFQScriptHandler::onStopRFQ] failed to find id {}", id);
      return;
   }
   itRFQ->second->deleteLater();
   rfqObjs_.erase(itRFQ);
}


RFQScriptRunner::RFQScriptRunner(const std::shared_ptr<MDCallbacksQt> &mdCallbacks
   , const std::shared_ptr<spdlog::logger> &logger
   , QObject *parent)
   : UserScriptRunner(logger, new RFQScriptHandler(logger, mdCallbacks, this), parent)
{
   thread_->setObjectName(QStringLiteral("RFQScriptRunner"));

   connect((RFQScriptHandler *)script_, &RFQScriptHandler::sendRFQ, this
      , &RFQScriptRunner::sendRFQ);
   connect((RFQScriptHandler *)script_, &RFQScriptHandler::cancelRFQ, this
      , &RFQScriptRunner::cancelRFQ);
}

RFQScriptRunner::~RFQScriptRunner() = default;

void RFQScriptRunner::submitRFQ(const std::string &id, const QString &symbol
   , double amount, bool buy)
{
   ((RFQScriptHandler *)script_)->submitRFQ(id, symbol, amount, buy);
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
