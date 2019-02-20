#ifndef __OHLC_HISTORY_API_SERVER_CONNECTION_LISTENER_H__
#define __OHLC_HISTORY_API_SERVER_CONNECTION_LISTENER_H__

#include "ServerConnectionListener.h"
#include "ServerConnection.h"
#include "DataPointsLocal.h"

class OhlcHistoryApiServerConnectionListener :
	public ServerConnectionListener
{
public:
	OhlcHistoryApiServerConnectionListener(const std::shared_ptr<ServerConnection>& connection
										   , const std::shared_ptr<spdlog::logger>& logger);
	~OhlcHistoryApiServerConnectionListener() noexcept override = default;

	OhlcHistoryApiServerConnectionListener(const OhlcHistoryApiServerConnectionListener&) = delete;
	OhlcHistoryApiServerConnectionListener& operator = (const OhlcHistoryApiServerConnectionListener&) = delete;

	OhlcHistoryApiServerConnectionListener(OhlcHistoryApiServerConnectionListener&&) = delete;
	OhlcHistoryApiServerConnectionListener& operator = (OhlcHistoryApiServerConnectionListener&&) = delete;

private:
	void OnDataFromClient(const std::string& clientId, const std::string& data) override;
	void OnClientConnected(const std::string& clientId) override;
	void OnClientDisconnected(const std::string& clientId) override;

private:
	std::shared_ptr<ServerConnection>	serverConnection_;
	std::shared_ptr<DataPointsLocal>       tradesDb_;
};

#endif // __OHLC_HISTORY_API_SERVER_CONNECTION_LISTENER_H__