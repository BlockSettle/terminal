#include "OhlcHistoryApiServerConnectionListener.h"

#include "OhlcHistory.pb.h"

using namespace ::google::protobuf;
using namespace Blocksettle::BS_MD::TradeHistoryServer;

OhlcHistoryApiServerConnectionListener::OhlcHistoryApiServerConnectionListener(const std::shared_ptr<ServerConnection>& connection
																			   , const std::shared_ptr<spdlog::logger>& logger)
	: serverConnection_(connection)
{
	tradesDb_ = std::make_shared<DataPointsLocal>("127.0.0.1", "3306", "mdhs", "root", "blank", logger);
}

void OhlcHistoryApiServerConnectionListener::OnDataFromClient(const std::string& clientId, const std::string& data)
{
	OhlcRequest request;
	if (request.ParseFromString(data))
	{
		auto vResult = tradesDb_->getDataPoints(request.market()
			, static_cast<DataPointsLocal::Interval>(request.interval())
			, request.limit());

		OhlcResponse response;
		for (auto iter = vResult.begin(); iter != vResult.end(); ++iter)
		{
			RepeatedPtrField<OhlcCandle>* pCandles = response.mutable_candles();
			pCandles->Reserve(vResult.size());

			OhlcCandle* pCandle = pCandles->Add();
			pCandle->set_timestamp((*iter)->timestamp);
			pCandle->set_open((*iter)->open);
			pCandle->set_high((*iter)->high);
			pCandle->set_low((*iter)->low);
			pCandle->set_close((*iter)->close);
		}

		serverConnection_->SendDataToClient(clientId, response.SerializeAsString());
	}
}

void OhlcHistoryApiServerConnectionListener::OnClientConnected(const std::string& clientId)
{
}

void OhlcHistoryApiServerConnectionListener::OnClientDisconnected(const std::string& clientId)
{
}
