#include "MdhsClient.h"
#include "spdlog/logger.h"
#include "ApplicationSettings.h"
#include "ConnectionManager.h"
#include "RequestReplyCommand.h"
#include "market_data_history.pb.h"

// Private Market
const std::string ANT_XBT = "ANT/XBT";
const std::string BLK_XBT = "BLK/XBT";
const std::string BSP_XBT = "BSP/XBT";
const std::string JAN_XBT = "JAN/XBT";
const std::string SCO_XBT = "SCO/XBT";
// Spot XBT
const std::string XBT_EUR = "XBT/EUR";
const std::string XBT_GBP = "XBT/GBP";
const std::string XBT_JPY = "XBT/JPY";
const std::string XBT_SEK = "XBT/SEK";
// Spot FX
const std::string EUR_GBP = "EUR/GBP";
const std::string EUR_JPY = "EUR/JPY";
const std::string EUR_SEK = "EUR/SEK";
const std::string GPB_JPY = "GPB/JPY";
const std::string GBP_SEK = "GBP/SEK";
const std::string JPY_SEK = "JPY/SEK";

static const std::map<std::string, MdhsClient::ProductType> PRODUCT_TYPES = {
   // Private Market
   { ANT_XBT, MdhsClient::ProductTypePrivateMarket },
   { BLK_XBT, MdhsClient::ProductTypePrivateMarket },
   { BSP_XBT, MdhsClient::ProductTypePrivateMarket },
   { JAN_XBT, MdhsClient::ProductTypePrivateMarket },
   { SCO_XBT, MdhsClient::ProductTypePrivateMarket },
   // Spot XBT
   { XBT_EUR, MdhsClient::ProductTypeXBT },
   { XBT_GBP, MdhsClient::ProductTypeXBT },
   { XBT_JPY, MdhsClient::ProductTypeXBT },
   { XBT_SEK, MdhsClient::ProductTypeXBT },
   // Spot FX
   { EUR_GBP, MdhsClient::ProductTypeFX },
   { EUR_JPY, MdhsClient::ProductTypeFX },
   { EUR_SEK, MdhsClient::ProductTypeFX },
   { GPB_JPY, MdhsClient::ProductTypeFX },
   { GBP_SEK, MdhsClient::ProductTypeFX },
   { JPY_SEK, MdhsClient::ProductTypeFX }
};

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

void MdhsClient::SendRequest(const MarketDataHistoryRequest& request)
{
   const auto apiConnection = connectionManager_->CreateGenoaClientConnection();
   command_ = std::make_shared<RequestReplyCommand>("MdhsClient", apiConnection, logger_);

   command_->SetReplyCallback([this](const std::string& data) -> bool
   {
      return OnDataReceived(data);
   });

   command_->SetErrorCallback([this](const std::string& message)
   {
      logger_->error("Failed to get history data from mdhs: {}", message);
      command_->CleanupCallbacks();
   });

   if (!command_->ExecuteRequest(
      //appSettings_->get<std::string>(ApplicationSettings::mdhsHost),
      //appSettings_->get<std::string>(ApplicationSettings::mdhsPort),
      "localhost",
      "5000",
      request.SerializeAsString()))
   {
      logger_->error("Failed to send request for mdhs.");
      command_->CleanupCallbacks();
   }
}

const MdhsClient::ProductType MdhsClient::GetProductType(const QString &product) const
{
   auto found = PRODUCT_TYPES.find(product.toStdString());
   if (found == PRODUCT_TYPES.end())
      return MdhsClient::ProductTypeUnknown;
   return found->second;
}

const bool MdhsClient::OnDataReceived(const std::string& data)
{
   emit DataReceived(data);
   command_->CleanupCallbacks();
   return true;
}
