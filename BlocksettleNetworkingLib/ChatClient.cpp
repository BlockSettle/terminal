#define SPDLOG_DEBUG_ON

#include "ChatClient.h"

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
    qDebug() << "ChatClient constructed!";

    heartbeatTimer_->setInterval(30 * 1000);
    heartbeatTimer_->setSingleShot(false);

    connect(heartbeatTimer_.get(), &QTimer::timeout, this, &ChatClient::sendHeartbeat);

}


void ChatClient::loginToServer(const std::string& hostname, const std::string& port
    , const std::string& login/*, const std::string& password*/)
{
    if (connection_) {
        qDebug() << "[ChatClient::loginToServer] connecting with not purged connection";
//       logger_->error("[ChatClient::loginToServer] connecting with not purged connection");
       return;
    }

    connection_ = connectionManager_->CreateSecuredDataConnection();
    connection_->SetServerPublicKey(serverPublicKey_);
    if (!connection_->openConnection(hostname, port, this))
    {
        qDebug() << "[ChatClient::loginToServer] failed to open ZMQ data connection";
        //logger_->error("[ChatClient::loginToServer] failed to open ZMQ data connection");
        connection_.reset();
    }
}


void ChatClient::sendHeartbeat()
{
//    SPDLOG_DEBUG(logger_, "[ChatClient] sending heartbeat");
}


void ChatClient::OnDataReceived(const std::string& data)
{
    qDebug() << "ChatClient::OnDataReceived]";
//    logger_->debug("[ChatClient::OnDataReceived]");
}


void ChatClient::OnConnected()
{
    qDebug() << "ChatClient::OnConnected]";
//    logger_->debug("[ChatClient::OnConnected]");
}


void ChatClient::OnDisconnected()
{
    qDebug() << "ChatClient::OnDisconnected]";
//    logger_->debug("[ChatClient::OnDisconnected]");
}


void ChatClient::OnError(DataConnectionError errorCode)
{
//    logger_->debug("[ChatClient::OnError]");
}
