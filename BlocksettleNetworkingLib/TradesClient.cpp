#include "TradesClient.h"

#include "spdlog/logger.h"

#include "ApplicationSettings.h"
#include "TradesDB.h"


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
