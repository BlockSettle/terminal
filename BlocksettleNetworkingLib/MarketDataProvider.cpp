#include "MarketDataProvider.h"

#include <spdlog/spdlog.h>

MarketDataProvider::MarketDataProvider(const std::shared_ptr<spdlog::logger>& logger)
   : logger_{logger}
{}

void MarketDataProvider::SetConnectionSettings(const std::string &host, const std::string &port)
{
   host_ = host;
   port_ = port;

   if (!host_.empty() && !port_.empty()) {
      emit CanSubscribe();
   } else {
      logger_->error("[MarketDataProvider::SetConnectionSettings] settings incompleted: \'{}:{}\'"
         , host, port);
   }
}

bool MarketDataProvider::SubscribeToMD()
{
   if (host_.empty() || port_.empty()) {
      logger_->error("[MarketDataProvider::SubscribeToMD] missing networking settings");
      return false;
   }

   emit UserWantToConnectToMD();

   return true;
}

void MarketDataProvider::MDLicenseAccepted()
{
   logger_->debug("[MarketDataProvider::MDLicenseAccepted] user accepted MD agreement. Start connection to {}:{}"
      , host_, port_);

   StartMDConnection();
}