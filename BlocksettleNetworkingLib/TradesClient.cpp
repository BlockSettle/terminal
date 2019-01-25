#include "TradesClient.h"

#include <vector>

#include <QDateTime>

#include "spdlog/logger.h"

#include "ApplicationSettings.h"
//#include "TradesDB.h"


TradesClient::TradesClient(const std::shared_ptr<ApplicationSettings>& appSettings
                           , const std::shared_ptr<spdlog::logger>& logger
                           , QObject* parent)
    : QObject(parent)
    , appSettings_(appSettings)
    , logger_(logger)
{
    tradesDb_ = std::make_unique<TradesDB>(logger, appSettings_->get<QString>(ApplicationSettings::tradesDbFile));
}

TradesClient::~TradesClient() noexcept
{
}

void TradesClient::init()
{
    tradesDb_->init();
}

const std::vector<TradesDB::DataPoint *> TradesClient::getRawPointDataArray(
        const QString &product
        , const QDateTime &sinceTime
        , const QDateTime &tillTime
        , qint64 stepDurationSecs)
{
    return tradesDb_->getDataPoints(product, sinceTime, tillTime, stepDurationSecs);
}

void TradesClient::onMDUpdated(bs::network::Asset::Type assetType
                               , const QString &security
                               , bs::network::MDFields fields)
{
    if (assetType == bs::network::Asset::Undefined) {
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
    tradesDb_->add(product, time, price, volume);
}
