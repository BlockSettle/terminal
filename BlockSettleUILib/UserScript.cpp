/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
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
#include "MarketDataProvider.h"
#include "UiUtils.h"
#include "Wallets/SyncWalletsManager.h"

#include <algorithm>


UserScript::UserScript(const std::shared_ptr<spdlog::logger> logger,
   std::shared_ptr<MarketDataProvider> mdProvider, QObject* parent)
   : QObject(parent)
   , logger_(logger)
   , engine_(new QQmlEngine(this))
   , component_(nullptr)
   , md_(new MarketData(mdProvider, this))
   , const_(new Constants(this))
   , storage_(new DataStorage(this))
{
   engine_->rootContext()->setContextProperty(QLatin1String("marketData"), md_);
   engine_->rootContext()->setContextProperty(QLatin1String("constants"), const_);
   engine_->rootContext()->setContextProperty(QLatin1String("dataStorage"), storage_);
}

UserScript::~UserScript()
{
   delete component_;
   component_ = nullptr;
}

void UserScript::load(const QString &filename)
{
   if (component_)  component_->deleteLater();
   component_ = new QQmlComponent(engine_, QUrl::fromLocalFile(filename), QQmlComponent::Asynchronous, this);
   if (!component_) {
      logger_->error("Failed to load component for file {}", filename.toStdString());
      emit failed(tr("Failed to load script %1").arg(filename));
   }
   else {
      connect(component_, &QQmlComponent::statusChanged, [this](QQmlComponent::Status status) {
         switch (status) {
         case QQmlComponent::Ready:
            emit loaded();
            break;
         case QQmlComponent::Error:
            emit failed(component_->errorString());
            break;
         default:    break;
         }
      });
   }

}

QObject *UserScript::instantiate()
{
   auto rv = component_->create();
   if (!rv)  emit failed(tr("Failed to instantiate: %1").arg(component_->errorString()));
   return rv;
}

void UserScript::setWalletsManager(std::shared_ptr<bs::sync::WalletsManager> walletsManager)
{
   const_->setWalletsManager(walletsManager);
}


//
// MarketData
//

MarketData::MarketData(std::shared_ptr<MarketDataProvider> mdProvider, QObject *parent)
   : QObject(parent)
{
   connect(mdProvider.get(), &MarketDataProvider::MDUpdate, this, &MarketData::onMDUpdated,
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


AutoQuoter::AutoQuoter(const std::shared_ptr<spdlog::logger> logger, const QString &filename
   , const std::shared_ptr<AssetManager> &assetManager
   , std::shared_ptr<MarketDataProvider> mdProvider, QObject* parent)
   : QObject(parent), script_(logger, mdProvider, this)
   , logger_(logger), assetManager_(assetManager)
{
   qmlRegisterType<BSQuoteReqReply>("bs.terminal", 1, 0, "BSQuoteReqReply");
   qmlRegisterUncreatableType<BSQuoteRequest>("bs.terminal", 1, 0, "BSQuoteRequest", tr("Can't create this type"));

   connect(&script_, &UserScript::loaded, [this] { emit loaded(); });
   connect(&script_, &UserScript::failed, [this](const QString &err) { emit failed(err); });

   script_.load(filename);
}

QObject *AutoQuoter::instantiate(const bs::network::QuoteReqNotification &qrn)
{
   QObject *rv = script_.instantiate();
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

void AutoQuoter::destroy(QObject *o)
{
   delete o;
}

void AutoQuoter::setWalletsManager(std::shared_ptr<bs::sync::WalletsManager> walletsManager)
{
   script_.setWalletsManager(walletsManager);
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
