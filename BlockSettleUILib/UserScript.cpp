/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "UserScript.h"

#include <algorithm>
#include <spdlog/logger.h>
#include <QJsonObject>
#include <QJsonDocument>
#include <QQmlComponent>
#include <QQmlContext>

#include "AssetManager.h"
#include "CurrencyPair.h"
#include "DataConnection.h"
#include "MDCallbacksQt.h"
#include "UiUtils.h"
#include "Wallets/SyncWalletsManager.h"
#include "UtxoReservationManager.h"

UserScript::UserScript(const std::shared_ptr<spdlog::logger> &logger,
   const std::shared_ptr<MDCallbacksQt> &mdCallbacks, const ExtConnections &extConns
   , QObject* parent)
   : QObject(parent)
   , logger_(logger)
   , engine_(new QQmlEngine(this))
   , component_(nullptr)
   , md_(mdCallbacks ? new MarketData(mdCallbacks, this) : nullptr)
   , extConns_(extConns)
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

   if (component_->isReady()) {
      emit loaded();
      return true;
   }
   if (component_->isError()) {
      logger_->error("Failed to load {}: {}", filename.toStdString()
         , component_->errorString().toStdString());
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

bool UserScript::sendExtConn(const QString &name, const QString &type, const QString &message)
{
   const auto &itConn = extConns_.find(name.toStdString());
   if (itConn == extConns_.end()) {
      logger_->error("[UserScript::sendExtConn] can't find external connector {}"
         , name.toStdString());
      return false;
   }
   if (!itConn->second->isActive()) {
      logger_->error("[UserScript::sendExtConn] external connector {} is not "
         "active", name.toStdString());
      return false;
   }
   QJsonParseError jsonError;
   auto jsonDoc = QJsonDocument::fromJson(QByteArray::fromStdString(message.toStdString())
      , &jsonError);
   if (jsonError.error != QJsonParseError::NoError) {
      logger_->error("[UserScript::sendExtConn] invalid JSON message: {}\n{}"
         , jsonError.errorString().toUtf8().toStdString(), message.toStdString());
      return false;
   }
   const auto messageObj = jsonDoc.object();
   QJsonObject jsonEnvelope;
   jsonEnvelope[QLatin1Literal("to")] = name;
   jsonEnvelope[QLatin1Literal("type")] = type;
   jsonEnvelope[QLatin1Literal("message")] = messageObj;
   jsonDoc.setObject(jsonEnvelope);
   const auto &msgJSON = jsonDoc.toJson(QJsonDocument::JsonFormat::Compact).toStdString();

   return itConn->second->send(msgJSON);
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
   , const std::shared_ptr<AssetManager> &assetManager
   , const std::shared_ptr<MDCallbacksQt> &mdCallbacks
   , const ExtConnections &extConns, QObject* parent)
   : UserScript(logger, mdCallbacks, extConns, parent)
   , assetManager_(assetManager)
{
   qmlRegisterType<BSQuoteReqReply>("bs.terminal", 1, 0, "BSQuoteReqReply");
   qmlRegisterUncreatableType<BSQuoteRequest>("bs.terminal", 1, 0, "BSQuoteRequest", tr("Can't create this type"));
}

QObject *AutoQuoter::instantiate(const bs::network::QuoteReqNotification &qrn)
{
   QObject *rv = UserScript::instantiate();
   if (rv) {
      BSQuoteReqReply *qrr = qobject_cast<BSQuoteReqReply *>(rv);
      qrr->init(logger_, assetManager_, this);

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

void BSQuoteReqReply::init(const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<AssetManager> &assetManager, UserScript *parent)
{
   logger_ = logger;
   assetManager_ = assetManager;
   parent_ = parent;
}

void BSQuoteReqReply::log(const QString &s)
{
   logger_->info("[BSQuoteReqReply] {}", s.toStdString());
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
   return assetManager_->getBalance(product.toStdString(), bs::UTXOReservationManager::kIncludeZcRequestor, nullptr);
}

bool BSQuoteReqReply::sendExtConn(const QString &name, const QString &type
   , const QString &message)
{
   return parent_->sendExtConn(name, type, message);
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
   if (!started_) {
      return;
   }

   auto &mdInfo = mdInfo_[security.toStdString()];
   mdInfo.merge(bs::network::MDField::get(mdFields));

   for (const auto &rfq : activeRFQs_) {
      const auto &sec = rfq.second->security();
      if (sec.isEmpty() || (security != sec)) {
         continue;
      }
      if (mdInfo.bidPrice > 0) {
         rfq.second->setIndicBid(mdInfo.bidPrice);
      }
      if (mdInfo.askPrice > 0) {
         rfq.second->setIndicAsk(mdInfo.askPrice);
      }
      if (mdInfo.lastPrice > 0) {
         rfq.second->setLastPrice(mdInfo.lastPrice);
      }
   }
}

SubmitRFQ *RFQScript::activeRFQ(const QString &id)
{
   if (!started_) {
      return nullptr;
   }
   const auto &itRFQ = activeRFQs_.find(id.toStdString());
   if (itRFQ == activeRFQs_.end()) {
      return nullptr;
   }
   return itRFQ->second;
}

void RFQScript::onAccepted(const std::string &id)
{
   if (!started_) {
      return;
   }
   const auto &itRFQ = activeRFQs_.find(id);
   if (itRFQ == activeRFQs_.end()) {
      return;
   }
   emit accepted(QString::fromStdString(id));
   emit itRFQ->second->accepted();
}

void RFQScript::onExpired(const std::string &id)
{
   if (!started_) {
      return;
   }
   const auto &itRFQ = activeRFQs_.find(id);
   if (itRFQ == activeRFQs_.end()) {
      logger_->warn("[RFQScript::onExpired] failed to find id {}", id);
      return;
   }
   emit expired(QString::fromStdString(id));
   emit itRFQ->second->expired();
}

void RFQScript::onCancelled(const std::string &id)
{
   if (!started_) {
      return;
   }
   const auto &itRFQ = activeRFQs_.find(id);
   if (itRFQ == activeRFQs_.end()) {
      return;
   }
   emit cancelled(QString::fromStdString(id));
   emit itRFQ->second->cancelled();
   itRFQ->second->deleteLater();
   activeRFQs_.erase(itRFQ);
}


AutoRFQ::AutoRFQ(const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<MDCallbacksQt> &mdCallbacks, QObject* parent)
   : UserScript(logger, mdCallbacks, {}, parent)
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
