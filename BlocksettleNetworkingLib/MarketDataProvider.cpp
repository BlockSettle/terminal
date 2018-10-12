#include "MarketDataProvider.h"

#include <spdlog/spdlog.h>

MarketDataProvider::MarketDataProvider(const std::shared_ptr<spdlog::logger>& logger)
   : logger_{logger}
{}

void MarketDataProvider::SubscribeToMD()
{
   emit UserWantToConnectToMD();
}

void MarketDataProvider::MDLicenseAccepted()
{
   logger_->debug("[MarketDataProvider::MDLicenseAccepted] user accepted MD agreement. Start connection");

   StartMDConnection();
}