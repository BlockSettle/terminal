#include "MarketDataProvider.h"

#include <spdlog/spdlog.h>

MarketDataProvider::MarketDataProvider(const std::shared_ptr<spdlog::logger>& logger)
   : logger_{logger}
{}

void MarketDataProvider::SubscribeToMD(const std::string &host, const std::string &port)
{
   emit UserWantToConnectToMD(host, port);
}

void MarketDataProvider::MDLicenseAccepted(const std::string &host, const std::string &port)
{
   logger_->debug("[MarketDataProvider::MDLicenseAccepted] user accepted MD agreement. Start connection to {}:{}"
      , host, port);

   StartMDConnection(host, port);
}