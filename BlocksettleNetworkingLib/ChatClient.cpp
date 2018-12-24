#include "ChatClient.h"
#include "ChatProtocol.h"

#include <spdlog/spdlog.h>
#include "botan/base64.h"

#include "ZmqSecuredDataConnection.h"
#include "ConnectionManager.h"
#include "ApplicationSettings.h"
#include "EncryptUtils.h"

#include <QDateTime>

#include <QDebug>



ChatClient::ChatClient(const std::shared_ptr<ConnectionManager>& connectionManager
                  , const std::shared_ptr<ApplicationSettings> &appSettings
                  , const std::shared_ptr<spdlog::logger>& logger)
   : connectionManager_(connectionManager)
   , appSettings_(appSettings)
   , logger_(logger)
   , heartbeatTimer_(new QTimer(this))
{
   heartbeatTimer_->setInterval(30 * 1000);
   heartbeatTimer_->setSingleShot(false);

   connect(heartbeatTimer_.get(), &QTimer::timeout, this, &ChatClient::sendHeartbeat);
}


std::string ChatClient::loginToServer(const std::string& email, const std::string& jwt)
{
   if (connection_) {
      logger_->error("[ChatClient::loginToServer] connecting with not purged connection");
      return std::string();
   }

   auto bytesHash = autheid::getSHA256(email.c_str(), email.size());
   currentUserId_ = Botan::base64_encode(bytesHash.data(), 8);
   currentChatId_ = currentUserId_;

   connection_ = connectionManager_->CreateSecuredDataConnection();
   connection_->SetServerPublicKey(appSettings_->get<std::string>(ApplicationSettings::chatServerPubKey));
   if (!connection_->openConnection(appSettings_->get<std::string>(ApplicationSettings::chatServerHost)
                            , appSettings_->get<std::string>(ApplicationSettings::chatServerPort), this))
   {
      logger_->error("[ChatClient::loginToServer] failed to open ZMQ data connection");
      connection_.reset();
   }

   auto loginRequest = std::make_shared<Chat::LoginRequest>("", currentUserId_, jwt);
   sendRequest(loginRequest);

   // [TODO]: Request users list after successfull login
   auto usersListRequest = std::make_shared<Chat::OnlineUsersRequest>("", currentUserId_);
   sendRequest(usersListRequest);

   heartbeatTimer_->start();
   return currentUserId_;
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
   // [TODO]: Add bye request

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

   auto users = response.getDataList();
   QList<QString> usersList;
   foreach(auto userId, users) {

      emit UserUpdate(QString::fromStdString(userId));
   }
}


QString ChatClient::prependMessage(const QString& messageText, const QString& senderId)
{
   QString displayMessage = QStringLiteral("[")
         + ( senderId.isEmpty() ? QString::fromStdString(currentUserId_) : senderId )
         + QStringLiteral("]: ") + messageText;
   return displayMessage;
}


void ChatClient::OnMessages(Chat::MessagesResponse& response)
{
   logger_->debug("Received messages from server: {}", response.getData());

   auto messages = response.getDataList();

   std::for_each(messages.begin(), messages.end(), [&](const std::string& msgData) {

      auto receivedMessage = Chat::MessageData::fromJSON(msgData);

      emit MessageUpdate(receivedMessage->getDateTime()
                      , prependMessage(receivedMessage->getMessageData()
                      , receivedMessage->getSenderId()));
   });
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


void ChatClient::onSendMessage(const QString& message)
{
   logger_->debug("[ChatClient::sendMessage] {}", message.toStdString());

   Chat::MessageData msg(QString::fromStdString(currentUserId_)
                    , QString::fromStdString(currentChatId_)
                    , QDateTime::currentDateTimeUtc()
                    , message);

   auto request = std::make_shared<Chat::SendMessageRequest>("", msg.toJsonString());
   sendRequest(request);
}


void ChatClient::onSetCurrentPrivateChat(const QString& userId)
{
   currentChatId_ = userId.toStdString();
   logger_->debug("Current chat changed: {}", currentChatId_);

   auto request = std::make_shared<Chat::MessagesRequest>("", currentUserId_, currentChatId_);
   sendRequest(request);
}
