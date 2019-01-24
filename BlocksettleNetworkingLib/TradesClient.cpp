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
