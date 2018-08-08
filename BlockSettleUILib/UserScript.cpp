#include "UserScript.h"
#include <spdlog/logger.h>
#include <QQmlComponent>
#include <QtConcurrent/QtConcurrentRun>
#include "AssetManager.h"
#include "CurrencyPair.h"
#include "MarketDataProvider.h"


UserScript::UserScript(const std::shared_ptr<spdlog::logger> logger,
   std::shared_ptr<MarketDataProvider> mdProvider, QObject* parent)
   : QObject(parent)
   , logger_(logger)
   , engine_(new QQmlEngine(this))
   , ctx_(new QQmlContext(engine_, this))
   , component_(nullptr)
   , md_(new MarketData(mdProvider, this))
{
   ctx_->setContextProperty(QLatin1String("marketData"), md_);
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


//
// MarketData
//

MarketData::MarketData(std::shared_ptr<MarketDataProvider> mdProvider, QObject *parent)
   : QObject(parent)
{
   connect(mdProvider.get(), &MarketDataProvider::MDUpdate, this, &MarketData::onMDUpdated);
}

double MarketData::bid(const QString &sec) const
{
   auto it = data_.find(sec);

   if (it != data_.cend()) {
      return it->second.first;
   } else {
      return 0.0;
   }
}

double MarketData::ask(const QString &sec) const
{
   auto it = data_.find(sec);

   if (it != data_.cend()) {
      return it->second.second;
   } else {
      return 0.0;
   }
}

void MarketData::onMDUpdated(bs::network::Asset::Type, const QString &security,
   bs::network::MDFields data)
{
   std::pair<double, double> prices;

   for (const auto &field : data) {
      switch (field.type) {
         case bs::network::MDField::PriceBid:
            prices.first = field.value;
            break;

         case bs::network::MDField::PriceOffer:
            prices.second = field.value;
            break;

         default:  break;
      }
   }

   data_[security] = prices;
}


AutoQuoter::AutoQuoter(const std::shared_ptr<spdlog::logger> logger, const QString &filename
   , const std::shared_ptr<AssetManager> &assetManager
   , std::shared_ptr<MarketDataProvider> mdProvider, QObject* parent)
   : QObject(parent), script_(logger, mdProvider, this), logger_(logger), assetManager_(assetManager)
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
   }
   return rv;
}

void AutoQuoter::destroy(QObject *o)
{
   delete o;
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
