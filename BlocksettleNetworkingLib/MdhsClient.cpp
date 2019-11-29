/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "MdhsClient.h"

#include "ApplicationSettings.h"
#include "ConnectionManager.h"
#include "FastLock.h"
#include "RequestReplyCommand.h"

#include "market_data_history.pb.h"

#include <spdlog/logger.h>

MdhsClient::MdhsClient(
   const std::shared_ptr<ApplicationSettings>& appSettings,
   const std::shared_ptr<ConnectionManager>& connectionManager,
   const std::shared_ptr<spdlog::logger>& logger,
   QObject* pParent)
   : QObject(pParent)
   , appSettings_(appSettings)
   , connectionManager_(connectionManager)
   , logger_(logger)
{
}

MdhsClient::~MdhsClient() noexcept = default;

void MdhsClient::SendRequest(const MarketDataHistoryRequest& request)
{
   requestId_ += 1;
   int requestId = requestId_;

   auto apiConnection = connectionManager_->CreateGenoaClientConnection();
   auto command = std::make_unique<RequestReplyCommand>("MdhsClient", apiConnection, logger_);

   command->SetReplyCallback([requestId, this](const std::string& data) -> bool {
      QMetaObject::invokeMethod(this, [this, requestId, data] {
         activeCommands_.erase(requestId);
         emit DataReceived(data);
      });
      return true;
   });

   command->SetErrorCallback([requestId, this](const std::string& message) {
      logger_->error("Failed to get history data from mdhs: {}", message);
      QMetaObject::invokeMethod(this, [this, requestId] {
         activeCommands_.erase(requestId);
      });
   });

   if (!command->ExecuteRequest(
      appSettings_->get<std::string>(ApplicationSettings::mdhsHost),
      appSettings_->get<std::string>(ApplicationSettings::mdhsPort),
      request.SerializeAsString()))
   {
      logger_->error("Failed to send request for mdhs.");
      return;
   }

   activeCommands_.emplace(requestId, std::move(command));
}
