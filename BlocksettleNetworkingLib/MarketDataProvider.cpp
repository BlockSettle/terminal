/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
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
      if (waitingForConnectionDetails_) {
         waitingForConnectionDetails_ = false;
         StartMDConnection();
      } else {
         logger_->debug("[MarketDataProvider::SetConnectionSettings] connection settings loaded {} : {}"
            , host_, port_);
      }
   } else {
      logger_->error("[MarketDataProvider::SetConnectionSettings] settings incompleted: \'{}:{}\'"
         , host, port);
   }
}

void MarketDataProvider::SubscribeToMD()
{
   emit UserWantToConnectToMD();
}

void MarketDataProvider::UnsubscribeFromMD()
{
   StopMDConnection();
}

void MarketDataProvider::MDLicenseAccepted()
{
   if (host_.empty() || port_.empty()) {
      logger_->debug("[MarketDataProvider::MDLicenseAccepted] need to load connection settings before connect");
      waitingForConnectionDetails_ = true;
      WaitingForConnectionDetails();
      return;
   }

   logger_->debug("[MarketDataProvider::MDLicenseAccepted] user accepted MD agreement. Start connection to {}:{}"
      , host_, port_);

   StartMDConnection();
}