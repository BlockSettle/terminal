#include "ChatClient.h"

#include <spdlog/spdlog.h>

#include "ZmqSecuredDataConnection.h"
#include "ConnectionManager.h"


#include <QDebug>



ChatClient::ChatClient(const std::shared_ptr<ConnectionManager>& connectionManager)
    : connectionManager_(connectionManager)
    , heartbeatTimer_(new QTimer(this))
{
    std::cout << "ChatClient constructed!";

    heartbeatTimer_->setInterval(30 * 1000);
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
    if (!connection_->openConnection(hostname, port, this))
    {
        logger_->error("[ChatClient::loginToServer] failed to open ZMQ data connection");
        connection_.reset();
    }
}


void ChatClient::sendHeartbeat()
{
//    SPDLOG_DEBUG(logger_, "[ChatClient] sending heartbeat");
}


void ChatClient::OnDataReceived(const std::string& data)
{
//    logger_->debug("[ChatClient::OnDataReceived]");
}


void ChatClient::OnConnected()
{
//    logger_->debug("[ChatClient::OnConnected]");
}


void ChatClient::OnDisconnected()
{
//    logger_->debug("[ChatClient::OnDisconnected]");
}


void ChatClient::OnError(DataConnectionError errorCode)
{
//    logger_->debug("[ChatClient::OnError]");
}
