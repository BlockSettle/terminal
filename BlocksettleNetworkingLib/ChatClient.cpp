#define SPDLOG_DEBUG_ON

#include "ChatClient.h"
#include "ChatProtocol.h"

#include <spdlog/spdlog.h>

#include "ZmqSecuredDataConnection.h"
#include "ConnectionManager.h"


#include <QDebug>



ChatClient::ChatClient(const std::shared_ptr<ConnectionManager>& connectionManager
                       , const std::shared_ptr<ApplicationSettings> &appSettings
                       , const std::shared_ptr<spdlog::logger>& logger
                       , const std::string& serverPublicKey)
    : connectionManager_(connectionManager)
    , appSettings_(appSettings)
    , logger_(logger)
    , serverPublicKey_(serverPublicKey)
    , heartbeatTimer_(new QTimer(this))
{
    connectionManager_ = std::make_shared<ConnectionManager>(logger_);

    heartbeatTimer_->setInterval(5 * 1000);
    heartbeatTimer_->setSingleShot(false);

    connect(heartbeatTimer_.get(), &QTimer::timeout, this, &ChatClient::sendHeartbeat);

}


void ChatClient::loginToServer(const std::string& hostname, const std::string& port
    , const std::string& login/*, const std::string& password*/)
{
    if (connection_) {
       logger_->error("[ChatClient::loginToServer] connecting with not purged connection");
       return;
    }

    connection_ = connectionManager_->CreateSecuredDataConnection();
    connection_->SetServerPublicKey(serverPublicKey_);
    if (!connection_->openConnection(hostname, port, this))
    {
        logger_->error("[ChatClient::loginToServer] failed to open ZMQ data connection");
        connection_.reset();
    }

    heartbeatTimer_->start();
}


void ChatClient::sendRequest(const std::shared_ptr<Chat::Request>& request)
{
    auto requestData = request->getData();

    logger_->debug("[ChatClient::sendRequest] \"{}\"", requestData.c_str());

    if (!connection_->isActive())
    {
        logger_->error("Connection is not alive!");
    }

    connection_->send(requestData);
}


void ChatClient::sendHeartbeat()
{
    auto request = std::make_shared<Chat::HeartbeatPingRequest>("user1");
    auto data = request->getData();
    sendRequest(request);
}


void ChatClient::OnDataReceived(const std::string& data)
{
    logger_->debug("[ChatClient::OnDataReceived] {}", data);
}


void ChatClient::OnConnected()
{
    logger_->debug("[ChatClient::OnConnected]");
}


void ChatClient::OnDisconnected()
{
    logger_->debug("[ChatClient::OnDisconnected]");
}


void ChatClient::OnError(DataConnectionError errorCode)
{
    logger_->debug("[ChatClient::OnError] {}", errorCode);
}
