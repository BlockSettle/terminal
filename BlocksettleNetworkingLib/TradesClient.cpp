#include "TradesClient.h"

#include <vector>

#include <QDateTime>
#include <QRandomGenerator>

#include "spdlog/logger.h"

#include "ApplicationSettings.h"

// Private Market
const std::string ANT_XBT = "ANT/XBT";
const std::string BLK_XBT = "BLK/XBT";
const std::string BSP_XBT = "BSP/XBT";
const std::string JAN_XBT = "JAN/XBT";
const std::string SCO_XBT = "SCO/XBT";
// Spot XBT
const std::string XBT_EUR = "XBT/EUR";
const std::string XBT_GBP = "XBT/GBP";
const std::string XBT_JPY = "XBT/JPY";
const std::string XBT_SEK = "XBT/SEK";
// Spot FX
const std::string EUR_GBP = "EUR/GBP";
const std::string EUR_JPY = "EUR/JPY";
const std::string EUR_SEK = "EUR/SEK";
const std::string GPB_JPY = "GPB/JPY";
const std::string GBP_SEK = "GBP/SEK";
const std::string JPY_SEK = "JPY/SEK";

static const std::map<std::string, TradesClient::ProductType> PRODUCT_TYPES = {
   // Private Market
   { ANT_XBT, TradesClient::ProductTypePrivateMarket },
   { BLK_XBT, TradesClient::ProductTypePrivateMarket },
   { BSP_XBT, TradesClient::ProductTypePrivateMarket },
   { JAN_XBT, TradesClient::ProductTypePrivateMarket },
   { SCO_XBT, TradesClient::ProductTypePrivateMarket },
   // Spot XBT
   { XBT_EUR, TradesClient::ProductTypeXBT },
   { XBT_GBP, TradesClient::ProductTypeXBT },
   { XBT_JPY, TradesClient::ProductTypeXBT },
   { XBT_SEK, TradesClient::ProductTypeXBT },
   // Spot FX
   { EUR_GBP, TradesClient::ProductTypeFX },
   { EUR_JPY, TradesClient::ProductTypeFX },
   { EUR_SEK, TradesClient::ProductTypeFX },
   { GPB_JPY, TradesClient::ProductTypeFX },
   { GBP_SEK, TradesClient::ProductTypeFX },
   { JPY_SEK, TradesClient::ProductTypeFX }
};


TradesClient::TradesClient(const std::shared_ptr<ApplicationSettings>& appSettings
                           , const std::shared_ptr<spdlog::logger>& logger
                           , QObject* parent)
    : QObject(parent)
    , appSettings_(appSettings)
    , logger_(logger)
{
   /*const std::string databaseHost = "127.0.0.1";
   const std::string databasePort = "3306";
   const std::string databaseName = "mdhs";
   const std::string databaseUser = "mdhs";
   const std::string databasePassword = "0000";
   tradesDb_ = std::make_unique<DataPointsLocal>(databaseHost
                                                 , databasePort
                                                 , databaseName
                                                 , databaseUser
                                                 , databasePassword
                                                 , logger);*/
}

TradesClient::~TradesClient() noexcept
{
}

void TradesClient::init()
{
//    tradesDb_->init();
}

const std::vector<DataPointsLocal::DataPoint *> TradesClient::getRawPointDataArray(
        const QString &product
        , DataPointsLocal::Interval interval
        , qint64 maxCount)
{
//   return tradesDb_->getDataPoints(product.toStdString(), interval, maxCount);
   std::vector<DataPointsLocal::DataPoint *> result;
   auto timestamp = QDateTime::currentDateTimeUtc();
   for (int i = 0; i < maxCount; ++i) {
      auto last = i == 0 ? 0 : result.at(i - 1);
      result.push_back(generatePoint(timestamp.toMSecsSinceEpoch(), last));
      switch (interval) {
      case DataPointsLocal::OneYear:
         timestamp = timestamp.addYears(-1);
         break;
      case DataPointsLocal::SixMonths:
         timestamp = timestamp.addMonths(-6);
         break;
      case DataPointsLocal::OneMonth:
         timestamp = timestamp.addMonths(-1);
         break;
      case DataPointsLocal::OneWeek:
         timestamp = timestamp.addDays(-timestamp.date().dayOfWeek());
         break;
      case DataPointsLocal::TwentyFourHours:
         timestamp = timestamp.addDays(-1);
         break;
      case DataPointsLocal::TwelveHours:
         timestamp = timestamp.addSecs(-3600*12);
         break;
      case DataPointsLocal::SixHours:
         timestamp = timestamp.addSecs(-3600*6);
         break;
      case DataPointsLocal::OneHour:
         timestamp = timestamp.addSecs(-3600);
         break;
      default:
         timestamp = timestamp.addSecs(-3600);
         break;
      }
   }
   return result;
}

TradesClient::ProductType TradesClient::getProductType(const QString &product) const
{
   auto found = PRODUCT_TYPES.find(product.toStdString());
   if (found == PRODUCT_TYPES.end()) {
      return TradesClient::ProductTypeUnknown;
   } else {
      return found->second;
   }
}

void TradesClient::onMDUpdated(bs::network::Asset::Type assetType
                               , const QString &security
                               , bs::network::MDFields fields)
{
    /*if (assetType == bs::network::Asset::Undefined) {
        return;
    }

    QDateTime time = QDateTime::currentDateTime();
    QString product = security;
    qreal price = -1.0;
    qreal volume = -1.0;
    for (const auto& field : fields) {
        switch (field.type) {
        case bs::network::MDField::PriceLast:
            price = field.value;
            break;
        case bs::network::MDField::DailyVolume:
            volume = field.value;
            break;
        default:
            break;
        }
    }
    if (price == -1.0 || volume == -1.0 || product.isEmpty()) {
        return;
    }
    tradesDb_->add(product, time, price, volume);*/
}

DataPointsLocal::DataPoint *TradesClient::generatePoint(
      qreal timestamp
      , DataPointsLocal::DataPoint *prev)
{
   static qreal minPrice = 0.0;
   qreal lower = 0.5;
   qreal upper = 1.0;
   qreal open = prev ? prev->close : minPrice;
   qreal high = qMax(minPrice, open - lower + QRandomGenerator::global()->generateDouble() * upper);
   qreal low = qMax(minPrice, open - lower + QRandomGenerator::global()->generateDouble() * upper);
   if (low > high) {
      std::swap(high, low);
   }
   qreal close = qMax(minPrice, open - lower + QRandomGenerator::global()->generateDouble() * upper);
   qreal volume = QRandomGenerator::global()->generateDouble() * 100;
   return new DataPointsLocal::DataPoint(open, high, low, close, volume, timestamp);
}
