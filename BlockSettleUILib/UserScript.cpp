/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "UserScript.h"
#include <spdlog/logger.h>
#include <QQmlComponent>
#include <QQmlContext>
#include "AssetManager.h"
#include "CurrencyPair.h"
#include "MDCallbacksQt.h"
#include "UiUtils.h"
#include "Wallets/SyncWalletsManager.h"

#include <algorithm>


UserScript::UserScript(const std::shared_ptr<spdlog::logger> &logger,
   const std::shared_ptr<MDCallbacksQt> &mdCallbacks, QObject* parent)
   : QObject(parent)
   , logger_(logger)
   , engine_(new QQmlEngine(this))
   , component_(nullptr)
   , md_(mdCallbacks ? new MarketData(mdCallbacks, this) : nullptr)
   , const_(new Constants(this))
   , storage_(new DataStorage(this))
{
   if (md_) {
      engine_->rootContext()->setContextProperty(QLatin1String("marketData"), md_);
   }
   engine_->rootContext()->setContextProperty(QLatin1String("constants"), const_);
   engine_->rootContext()->setContextProperty(QLatin1String("dataStorage"), storage_);
}

UserScript::~UserScript()
{
   delete component_;
   component_ = nullptr;
}

bool UserScript::load(const QString &filename)
{
   if (component_)  component_->deleteLater();
   component_ = new QQmlComponent(engine_, QUrl::fromLocalFile(filename)
      , QQmlComponent::PreferSynchronous, this);
   if (!component_) {
      logger_->error("Failed to load component for file {}", filename.toStdString());
      emit failed(tr("Failed to load script %1").arg(filename));
      return false;
   }
/*   else {
      connect(component_, &QQmlComponent::statusChanged, [this](QQmlComponent::Status status) {
         switch (status) {
         case QQmlComponent::Ready:
            emit loaded();
            return;
         case QQmlComponent::Error:
            emit failed(component_->errorString());
            break;
         default:    break;
         }
      });
   }*/   // Switched to synchronous loading

   if (component_->isReady()) {
      emit loaded();
      return true;
   }
   if (component_->isError()) {
      emit failed(component_->errorString());
   }
   return false;
}

QObject *UserScript::instantiate()
{
   auto rv = component_->create();
   if (!rv) {
      emit failed(tr("Failed to instantiate: %1").arg(component_->errorString()));
   }
   return rv;
}

void UserScript::setWalletsManager(std::shared_ptr<bs::sync::WalletsManager> walletsManager)
{
   const_->setWalletsManager(walletsManager);
}


//
// MarketData
//

MarketData::MarketData(const std::shared_ptr<MDCallbacksQt> &mdCallbacks, QObject *parent)
   : QObject(parent)
{
   connect(mdCallbacks.get(), &MDCallbacksQt::MDUpdate, this, &MarketData::onMDUpdated,
      Qt::QueuedConnection);
}

double MarketData::bid(const QString &sec) const
{
   auto it = data_.find(sec);

   if (it != data_.cend()) {
      auto dit = it->second.find(bs::network::MDField::PriceBid);

      if (dit != it->second.cend()) {
         return dit->second;
      } else {
         return 0.0;
      }
   } else {
      return 0.0;
   }
}

double MarketData::ask(const QString &sec) const
{
   auto it = data_.find(sec);

   if (it != data_.cend()) {
      auto dit = it->second.find(bs::network::MDField::PriceOffer);

      if (dit != it->second.cend()) {
         return dit->second;
      } else {
         return 0.0;
      }
   } else {
      return 0.0;
   }
}

void MarketData::onMDUpdated(bs::network::Asset::Type, const QString &security,
   bs::network::MDFields data)
{
   for (const auto &field : data) {
      data_[security][field.type] = field.value;
   }
}


//
// DataStorage
//

DataStorage::DataStorage(QObject *parent)
   : QObject(parent)
{
}

double DataStorage::bought(const QString &currency)
{
   return std::accumulate(std::begin(bought_[currency]), std::end(bought_[currency]),
      0.0,
      [] (double value, const std::map<QString, double>::value_type& p)
         { return value + p.second; });
}

void DataStorage::setBought(const QString &currency, double v, const QString &id)
{
   bought_[currency][id] = v;
}

double DataStorage::sold(const QString &currency)
{
   return std::accumulate(std::begin(sold_[currency]), std::end(sold_[currency]),
      0.0,
      [] (double value, const std::map<QString, double>::value_type& p)
         { return value + p.second; });
}

void DataStorage::setSold(const QString &currency, double v, const QString &id)
{
   sold_[currency][id] = v;
}


//
// Constants
//

Constants::Constants(QObject *parent)
   : QObject(parent)
   , walletsManager_(nullptr)
{}

int Constants::payInTxSize() const
{
   return 125;
}

int Constants::payOutTxSize() const
{
   return 82;
}

float Constants::feePerByte()
{
   if (walletsManager_) {
      walletsManager_->estimatedFeePerByte(2, [this](float fee) { feePerByte_ = fee; }, this);
   }
   return feePerByte_;  //NB: sometimes returns previous value if previous call needs to wait for result from Armory
}

QString Constants::xbtProductName() const
{
   return UiUtils::XbtCurrency;
}

void Constants::setWalletsManager(std::shared_ptr<bs::sync::WalletsManager> walletsManager)
{
   walletsManager_ = walletsManager;
   walletsManager_->estimatedFeePerByte(2, [this](float fee) { feePerByte_ = fee; }, this);
}


AutoQuoter::AutoQuoter(const std::shared_ptr<spdlog::logger> &logger
   , const QString &filename
   , const std::shared_ptr<AssetManager> &assetManager
   , const std::shared_ptr<MDCallbacksQt> &mdCallbacks, QObject* parent)
   : UserScript(logger, mdCallbacks, parent)
   , assetManager_(assetManager)
{
   qmlRegisterType<BSQuoteReqReply>("bs.terminal", 1, 0, "BSQuoteReqReply");
   qmlRegisterUncreatableType<BSQuoteRequest>("bs.terminal", 1, 0, "BSQuoteRequest", tr("Can't create this type"));

   if (!load(filename)) {
      throw std::runtime_error("failed to load " + filename.toStdString());
   }
}

QObject *AutoQuoter::instantiate(const bs::network::QuoteReqNotification &qrn)
{
   QObject *rv = UserScript::instantiate();
   if (rv) {
      BSQuoteReqReply *qrr = qobject_cast<BSQuoteReqReply *>(rv);
      qrr->init(logger_, assetManager_);

      BSQuoteRequest *qr = new BSQuoteRequest(rv);
      qr->init(QString::fromStdString(qrn.quoteRequestId), QString::fromStdString(qrn.product)
         , (qrn.side == bs::network::Side::Buy), qrn.quantity, static_cast<int>(qrn.assetType));

      qrr->setQuoteReq(qr);
      qrr->setSecurity(QString::fromStdString(qrn.security));

      connect(qrr, &BSQuoteReqReply::sendingQuoteReply, [this](const QString &reqId, double price) {
         emit sendingQuoteReply(reqId, price);
      });
      connect(qrr, &BSQuoteReqReply::pullingQuoteReply, [this](const QString &reqId) {
         emit pullingQuoteReply(reqId);
      });

      qrr->start();
   }
   return rv;
}


void BSQuoteRequest::init(const QString &reqId, const QString &product, bool buy, double qty, int at)
{
   requestId_ = reqId;
   product_ = product;
   isBuy_ = buy;
   quantity_ = qty;
   assetType_ = at;
}

void BSQuoteReqReply::init(const std::shared_ptr<spdlog::logger> &logger, const std::shared_ptr<AssetManager> &assetManager)
{
   logger_ = logger;
   assetManager_ = assetManager;
}

void BSQuoteReqReply::log(const QString &s)
{
   logger_->info("[BSQuoteReply] {}", s.toStdString());
}

bool BSQuoteReqReply::sendQuoteReply(double price)
{
   QString reqId = quoteReq()->requestId();
   if (reqId.isEmpty())  return false;
   emit sendingQuoteReply(reqId, price);
   return true;
}

bool BSQuoteReqReply::pullQuoteReply()
{
   QString reqId = quoteReq()->requestId();
   if (reqId.isEmpty())  return false;
   emit pullingQuoteReply(reqId);
   return true;
}

QString BSQuoteReqReply::product()
{
   CurrencyPair cp(security().toStdString());
   return QString::fromStdString(cp.ContraCurrency(quoteReq()->product().toStdString()));
}

double BSQuoteReqReply::accountBalance(const QString &product)
{
   return assetManager_->getBalance(product.toStdString());
}


void SubmitRFQ::stop()
{
   emit stopRFQ(id_.toStdString());
}


void RFQScript::log(const QString &s)
{
   if (!logger_) {
      return;
   }
   logger_->info("[RFQScript] {}", s.toStdString());
}

SubmitRFQ *RFQScript::sendRFQ(const QString &symbol, bool buy, double amount)
{
   const auto id = CryptoPRNG::generateRandom(8).toHexStr();
   const auto submitRFQ = new SubmitRFQ(this);
   submitRFQ->setId(id);
   submitRFQ->setSecurity(symbol);
   submitRFQ->setAmount(amount);
   submitRFQ->setBuy(buy);

   activeRFQs_[id] = submitRFQ;
   emit sendingRFQ(submitRFQ);
   return submitRFQ;
}

void RFQScript::cancelRFQ(const std::string &id)
{
   const auto &itRFQ = activeRFQs_.find(id);
   if (itRFQ == activeRFQs_.end()) {
      logger_->error("[RFQScript::cancelRFQ] no active RFQ with id {}", id);
      return;
   }
   emit cancellingRFQ(id);
   itRFQ->second->deleteLater();
   activeRFQs_.erase(itRFQ);
}

void RFQScript::cancelAll()
{
   for (const auto &rfq : activeRFQs_) {
      emit cancellingRFQ(rfq.first);
      rfq.second->deleteLater();
   }
   activeRFQs_.clear();
}

void RFQScript::onMDUpdate(bs::network::Asset::Type, const QString &security,
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

   for (auto rfq : activeRFQs_) {
      const auto &sec = rfq.second->security();
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

void RFQScript::onAccepted(const std::string &id)
{
   const auto &itRFQ = activeRFQs_.find(id);
   if (itRFQ == activeRFQs_.end()) {
      return;
   }
   emit itRFQ->second->accepted();
}

void RFQScript::onExpired(const std::string &id)
{
   const auto &itRFQ = activeRFQs_.find(id);
   if (itRFQ == activeRFQs_.end()) {
      logger_->warn("[RFQScript::onExpired] failed to find id {}", id);
      return;
   }
   emit itRFQ->second->expired();
}

void RFQScript::onCancelled(const std::string &id)
{
   const auto &itRFQ = activeRFQs_.find(id);
   if (itRFQ == activeRFQs_.end()) {
      return;
   }
   emit itRFQ->second->cancelled();
   itRFQ->second->deleteLater();
   activeRFQs_.erase(itRFQ);
}


AutoRFQ::AutoRFQ(const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<MDCallbacksQt> &mdCallbacks, QObject* parent)
   : UserScript(logger, mdCallbacks, parent)
{
   qRegisterMetaType<SubmitRFQ *>();
   qmlRegisterType<SubmitRFQ>("bs.terminal", 1, 0, "SubmitRFQ");
   qmlRegisterType<RFQScript>("bs.terminal", 1, 0, "RFQScript");
}

QObject *AutoRFQ::instantiate()
{
   QObject *rv = UserScript::instantiate();
   if (!rv) {
      logger_->error("[AutoRFQ::instantiate] failed to instantiate script");
      return nullptr;
   }
   RFQScript *rfq = qobject_cast<RFQScript *>(rv);
   if (!rfq) {
      logger_->error("[AutoRFQ::instantiate] wrong script type");
      return nullptr;
   }
   rfq->init(logger_);

   connect(rfq, &RFQScript::sendingRFQ, this, &AutoRFQ::onSendRFQ);
   connect(rfq, &RFQScript::cancellingRFQ, this, &AutoRFQ::cancelRFQ);

   return rv;
}

void AutoRFQ::onSendRFQ(SubmitRFQ *rfq)
{
   if (!rfq) {
      logger_->error("[AutoRFQ::onSendRFQ] no RFQ passed");
      return;
   }
   connect(rfq, &SubmitRFQ::stopRFQ, this, &AutoRFQ::stopRFQ);
   emit sendRFQ(rfq->id().toStdString(), rfq->security(), rfq->amount(), rfq->buy());
}
