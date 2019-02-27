#include "MdhsClient.h"
#include "spdlog/logger.h"
#include "ApplicationSettings.h"
#include "ConnectionManager.h"
#include "RequestReplyCommand.h"
#include "OhlcHistory.pb.h"

//#include <vector>
//
//#include <QDateTime>
//#include <QRandomGenerator>
//

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

//void TradesClient::init()
//{
////    tradesDb_->init();
//}
//
//const std::vector<DataPointsLocal::DataPoint *> TradesClient::getRawPointDataArray(
//        const QString &product
//        , DataPointsLocal::Interval interval
//        , qint64 maxCount)
//{
////   return tradesDb_->getDataPoints(product.toStdString(), interval, maxCount);
//   std::vector<DataPointsLocal::DataPoint *> result;
//   auto timestamp = QDateTime::currentDateTimeUtc();
//   for (int i = 0; i < maxCount; ++i) {
//      auto last = i == 0 ? 0 : result.at(i - 1);
//      result.push_back(generatePoint(timestamp.toMSecsSinceEpoch(), last));
//      switch (interval) {
//      case DataPointsLocal::OneYear:
//         timestamp = timestamp.addYears(-1);
//         break;
//      case DataPointsLocal::SixMonths:
//         timestamp = timestamp.addMonths(-6);
//         break;
//      case DataPointsLocal::OneMonth:
//         timestamp = timestamp.addMonths(-1);
//         break;
//      case DataPointsLocal::OneWeek:
//         timestamp = timestamp.addDays(-timestamp.date().dayOfWeek());
//         break;
//      case DataPointsLocal::TwentyFourHours:
//         timestamp = timestamp.addDays(-1);
//         break;
//      case DataPointsLocal::TwelveHours:
//         timestamp = timestamp.addSecs(-3600*12);
//         break;
//      case DataPointsLocal::SixHours:
//         timestamp = timestamp.addSecs(-3600*6);
//         break;
//      case DataPointsLocal::OneHour:
//         timestamp = timestamp.addSecs(-3600);
//         break;
//      default:
//         timestamp = timestamp.addSecs(-3600);
//         break;
//      }
//   }
//   return result;
//}

void MdhsClient::SendRequest(const OhlcRequest& request)
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
	command_->CleanupCallbacks();
	emit DataReceived(data);
	return true;
}
