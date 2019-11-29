/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/

#include "UserScriptRunner.h"
#include "SignContainer.h"
#include "MarketDataProvider.h"
#include "UserScript.h"
#include "Wallets/SyncWalletsManager.h"

#include <QThread>
#include <QTimer>


//
// UserScriptHandler
//

UserScriptHandler::UserScriptHandler(std::shared_ptr<QuoteProvider> quoteProvider,
   std::shared_ptr<SignContainer> signingContainer,
   std::shared_ptr<MarketDataProvider> mdProvider,
   std::shared_ptr<AssetManager> assetManager,
   std::shared_ptr<spdlog::logger> logger,
   UserScriptRunner *runner)
   : signingContainer_(signingContainer)
   , mdProvider_(mdProvider)
   , assetManager_(assetManager)
   , logger_(logger)
   , aqEnabled_(false)
   , aqTimer_(new QTimer(this))
{
   connect(quoteProvider.get(), &QuoteProvider::quoteReqNotifReceived,
      this, &UserScriptHandler::onQuoteReqNotification, Qt::QueuedConnection);
   connect(quoteProvider.get(), &QuoteProvider::quoteNotifCancelled,
      this, &UserScriptHandler::onQuoteNotifCancelled, Qt::QueuedConnection);
   connect(quoteProvider.get(), &QuoteProvider::quoteCancelled,
      this, &UserScriptHandler::onQuoteReqCancelled, Qt::QueuedConnection);
   connect(quoteProvider.get(), &QuoteProvider::quoteRejected,
      this, &UserScriptHandler::onQuoteReqRejected, Qt::QueuedConnection);

   connect(runner, &UserScriptRunner::initAQ, this, &UserScriptHandler::initAQ,
      Qt::QueuedConnection);
   connect(runner, &UserScriptRunner::deinitAQ, this, &UserScriptHandler::deinitAQ,
      Qt::QueuedConnection);
   connect(mdProvider.get(), &MarketDataProvider::MDUpdate, this, &UserScriptHandler::onMDUpdate,
      Qt::QueuedConnection);
   connect(quoteProvider.get(), &QuoteProvider::bestQuotePrice,
      this, &UserScriptHandler::onBestQuotePrice, Qt::QueuedConnection);

   aqTimer_->setInterval(500);
   connect(aqTimer_, &QTimer::timeout, this, &UserScriptHandler::aqTick);
   aqTimer_->start();
}

UserScriptHandler::~UserScriptHandler() noexcept
{
   deinitAQ(false);
}

void UserScriptHandler::setWalletsManager(const std::shared_ptr<bs::sync::WalletsManager> &walletsManager)
{
   walletsManager_ = walletsManager;

   if (aq_) {
      aq_->setWalletsManager(walletsManager);
   }
}

void UserScriptHandler::onQuoteReqNotification(const bs::network::QuoteReqNotification &qrn)
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
            if (mdIt->second.bidPrice > 0) {
               qobject_cast<BSQuoteReqReply *>(obj)->setIndicBid(mdIt->second.bidPrice);
            }
            if (mdIt->second.askPrice > 0) {
               qobject_cast<BSQuoteReqReply *>(obj)->setIndicAsk(mdIt->second.askPrice);
            }
            if (mdIt->second.lastPrice > 0) {
               qobject_cast<BSQuoteReqReply *>(obj)->setLastPrice(mdIt->second.lastPrice);
            }
         }
      }
   }
   else {
      aqQuoteReqs_.erase(qrn.quoteRequestId);
      if (itAQObj != aqObjs_.end()) {
         if (aq_) {
            aq_->destroy(itAQObj->second);
         }
         aqObjs_.erase(qrn.quoteRequestId);
         bestQPrices_.erase(qrn.quoteRequestId);
      }
   }
}

void UserScriptHandler::onQuoteReqCancelled(const QString &reqId, bool userCancelled)
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

void UserScriptHandler::onQuoteNotifCancelled(const QString &reqId)
{
   onQuoteReqCancelled(reqId, true);
}

void UserScriptHandler::onQuoteReqRejected(const QString &reqId, const QString &)
{
   onQuoteReqCancelled(reqId, true);
}

void UserScriptHandler::initAQ(const QString &fileName)
{
   if (fileName.isEmpty()) {
      return;
   }

   aqEnabled_ = false;

   aq_ = new AutoQuoter(logger_, fileName, assetManager_, mdProvider_, this);
   if (walletsManager_) {
      aq_->setWalletsManager(walletsManager_);
   }
   connect(aq_, &AutoQuoter::loaded, [this, fileName] {
      emit aqScriptLoaded(fileName);
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
   connect(aq_, &AutoQuoter::sendingQuoteReply, this, &UserScriptHandler::onAQReply);
   connect(aq_, &AutoQuoter::pullingQuoteReply, this, &UserScriptHandler::onAQPull);
}

void UserScriptHandler::deinitAQ(bool deleteAq)
{
   if (!aq_) {
      return;
   }

   for (auto aqObj : aqObjs_) {
      aq_->destroy(aqObj.second);
   }
   aqObjs_.clear();
   aqEnabled_ = false;

   if (deleteAq) {
      aq_->deleteLater();
      aq_ = nullptr;
   }

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

void UserScriptHandler::onMDUpdate(bs::network::Asset::Type, const QString &security,
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

      if (bid > 0) {
         qobject_cast<BSQuoteReqReply *>(aqObj.second)->setIndicBid(bid);
      }
      if (ask > 0) {
         qobject_cast<BSQuoteReqReply *>(aqObj.second)->setIndicAsk(ask);
      }
      if (last > 0) {
         qobject_cast<BSQuoteReqReply *>(aqObj.second)->setLastPrice(last);
      }
   }
}

void UserScriptHandler::onBestQuotePrice(const QString reqId, double price, bool own)
{
   if (!own) {
      const auto itAQObj = aqObjs_.find(reqId.toStdString());
      if (itAQObj != aqObjs_.end()) {
         qobject_cast<BSQuoteReqReply *>(itAQObj->second)->setBestPrice(price);
      }
   }
}

void UserScriptHandler::onAQReply(const QString &reqId, double price)
{
   const auto itQRN = aqQuoteReqs_.find(reqId.toStdString());
   if (itQRN == aqQuoteReqs_.end()) {
      logger_->warn("[UserScriptHandler::onAQReply] QuoteReqNotification with id = {} not found", reqId.toStdString());
      return;
   }

   emit sendQuote(itQRN->second, price);
}

void UserScriptHandler::onAQPull(const QString &reqId)
{
   const auto itQRN = aqQuoteReqs_.find(reqId.toStdString());
   if (itQRN == aqQuoteReqs_.end()) {
      logger_->warn("[UserScriptHandler::onAQPull] QuoteReqNotification with id = {} not found", reqId.toStdString());
      return;
   }
   emit pullQuoteNotif(QString::fromStdString(itQRN->second.quoteRequestId), QString::fromStdString(itQRN->second.sessionToken));
}

void UserScriptHandler::aqTick()
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

UserScriptRunner::UserScriptRunner(std::shared_ptr<QuoteProvider> quoteProvider,
   std::shared_ptr<SignContainer> signingContainer,
   std::shared_ptr<MarketDataProvider> mdProvider,
   std::shared_ptr<AssetManager> assetManager,
   std::shared_ptr<spdlog::logger> logger,
   QObject *parent)
   : QObject(parent)
   , thread_(new QThread(this))
   , script_(new UserScriptHandler(quoteProvider, signingContainer,
         mdProvider, assetManager, logger, this))

   , logger_(logger)
{
   thread_->setObjectName(QStringLiteral("AQScriptRunner"));
   script_->moveToThread(thread_);

   connect(script_, &UserScriptHandler::aqScriptLoaded, this, &UserScriptRunner::aqScriptLoaded);
   connect(script_, &UserScriptHandler::failedToLoad, this, &UserScriptRunner::failedToLoad);
   connect(script_, &UserScriptHandler::pullQuoteNotif, this, &UserScriptRunner::pullQuoteNotif);
   connect(script_, &UserScriptHandler::sendQuote, this, &UserScriptRunner::sendQuote);

   thread_->start();
}

UserScriptRunner::~UserScriptRunner() noexcept
{
   script_->deleteLater();
   thread_->quit();
   thread_->wait();
}

void UserScriptRunner::setWalletsManager(const std::shared_ptr<bs::sync::WalletsManager> &walletsManager)
{
   script_->setWalletsManager(walletsManager);
}

void UserScriptRunner::enableAQ(const QString &fileName)
{
   logger_->info("Load AQ script {}...", fileName.toStdString());
   emit initAQ(fileName);
}

void UserScriptRunner::disableAQ()
{
   logger_->info("Unload AQ script");
   emit deinitAQ(true);
}
