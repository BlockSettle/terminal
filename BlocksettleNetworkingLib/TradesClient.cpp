#include "TradesClient.h"

#include <vector>

#include <QDateTime>

#include "spdlog/logger.h"

#include "ApplicationSettings.h"


TradesClient::TradesClient(const std::shared_ptr<ApplicationSettings>& appSettings
                           , const std::shared_ptr<spdlog::logger>& logger
                           , QObject* parent)
    : QObject(parent)
    , appSettings_(appSettings)
    , logger_(logger)
{
   const std::string databaseHost = "127.0.0.1";
   const std::string databasePort = "3306";
   const std::string databaseName = "mdhs";
   const std::string databaseUser = "mdhs";
   const std::string databasePassword = "0000";
   tradesDb_ = std::make_unique<DataPointsLocal>(databaseHost
                                                 , databasePort
                                                 , databaseName
                                                 , databaseUser
                                                 , databasePassword
                                                 , logger);
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
    return tradesDb_->getDataPoints(product.toStdString(), interval, maxCount);
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
