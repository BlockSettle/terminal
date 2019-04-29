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

MdhsClient::~MdhsClient() noexcept
{
   FastLock locker(lockCommands_);
   for (auto &cmd : activeCommands_) {
      cmd->DropResult();
   }
}

void MdhsClient::SendRequest(const MarketDataHistoryRequest& request)
{
   const auto apiConnection = connectionManager_->CreateGenoaClientConnection();
   auto command = std::make_shared<RequestReplyCommand>("MdhsClient", apiConnection, logger_);

   command->SetReplyCallback([command, this](const std::string& data) -> bool
   {
      command->CleanupCallbacks();
      FastLock locker(lockCommands_);
      activeCommands_.erase(command);
      return OnDataReceived(data);
   });

   command->SetErrorCallback([command, this](const std::string& message)
   {
      logger_->error("Failed to get history data from mdhs: {}", message);
      command->CleanupCallbacks();
      FastLock locker(lockCommands_);
      activeCommands_.erase(command);
   });

   {
      FastLock locker(lockCommands_);
      activeCommands_.emplace(command);
   }

   if (!command->ExecuteRequest(
      appSettings_->get<std::string>(ApplicationSettings::mdhsHost),
      appSettings_->get<std::string>(ApplicationSettings::mdhsPort),
      request.SerializeAsString()))
   {
      logger_->error("Failed to send request for mdhs.");
      command->CleanupCallbacks();
      FastLock locker(lockCommands_);
      activeCommands_.erase(command);
   }
}

bool MdhsClient::OnDataReceived(const std::string& data)
{
   emit DataReceived(data);
   return true;
}
