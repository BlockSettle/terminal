#include "ChatClient.h"
#include "ChatProtocol.h"

#include <spdlog/spdlog.h>

#include "ZmqSecuredDataConnection.h"
#include "ConnectionManager.h"
#include "ApplicationSettings.h"

#include <QDateTime>

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

    currentUserId_ = login;
    currentChatId_ = currentUserId_;

    connection_ = connectionManager_->CreateSecuredDataConnection();
    connection_->SetServerPublicKey(serverPublicKey_);
    if (!connection_->openConnection(hostname, port, this))
    {
        logger_->error("[ChatClient::loginToServer] failed to open ZMQ data connection");
        connection_.reset();
    }

    auto loginRequest = std::make_shared<Chat::LoginRequest>("", login, "");
    sendRequest(loginRequest);

    auto usersListRequest = std::make_shared<Chat::OnlineUsersRequest>("", login);
    sendRequest(usersListRequest);

    heartbeatTimer_->start();
}


void ChatClient::logout()
{
    if (!connection_.get()) {
       logger_->error("[ChatClient::logout] Disconnected already.");
       return;
    }

    currentUserId_ = "";
    heartbeatTimer_->stop();

    // Intentionally don't logout from server for testing reasons ...

    connection_.reset();
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
    auto request = std::make_shared<Chat::HeartbeatPingRequest>("");
    sendRequest(request);
}


void ChatClient::OnHeartbeatPong(Chat::HeartbeatPongResponse& response)
{
    logger_->debug("[ChatClient::OnHeartbeatPong] {}", response.getData());
}


void ChatClient::OnUsersList(Chat::UsersListResponse& response)
{
    logger_->debug("Received users list from server: {}", response.getData());

    auto users = response.getUsersList();
    QList<QString> usersList;
    foreach(auto userId, users) {
        emit OnUserUpdate(QString::fromStdString(userId));
        //usersList << QString::fromStdString(userId);
    }

    //emit OnUsersListUpdated(usersList);
}


void ChatClient::OnDataReceived(const std::string& data)
{
    logger_->debug("[ChatClient::OnDataReceived] {}", data);

    auto heartbeatResponse = Chat::Response::fromJSON(data);
    heartbeatResponse->handle(*this);
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


void ChatClient::sendMessage(const QString& message)
{
    std::string msg = message.toStdString();

    logger_->debug("[ChatClient::sendMessage] {}", msg);

    auto request = std::make_shared<Chat::SendMessageRequest>(""
                , currentUserId_, currentChatId_
                , std::to_string(QDateTime::currentDateTimeUtc().toMSecsSinceEpoch()), msg);

    sendRequest(request);
}


void ChatClient::setCurrentPrivateChat(const QString& userId)
{
    currentChatId_ = userId.toStdString();
    logger_->debug("Current chat changed: {}", currentChatId_);
}
