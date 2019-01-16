#include "ChatClient.h"
#include "ChatProtocol.h"

#include <spdlog/spdlog.h>
#include "botan/base64.h"

#include "ZmqSecuredDataConnection.h"
#include "ChatDB.h"
#include "ConnectionManager.h"
#include "ApplicationSettings.h"
#include "EncryptUtils.h"

#include <QDateTime>

Q_DECLARE_METATYPE(std::shared_ptr<Chat::MessageData>)
Q_DECLARE_METATYPE(std::vector<std::shared_ptr<Chat::MessageData>>)

ChatClient::ChatClient(const std::shared_ptr<ConnectionManager>& connectionManager
                  , const std::shared_ptr<ApplicationSettings> &appSettings
                  , const std::shared_ptr<spdlog::logger>& logger)

   : connectionManager_(connectionManager)
   , appSettings_(appSettings)
   , logger_(logger)
{
   qRegisterMetaType<std::shared_ptr<Chat::MessageData>>();
   qRegisterMetaType<std::vector<std::shared_ptr<Chat::MessageData>>>();

   chatDb_ = std::make_unique<ChatDB>(logger, appSettings_->get<QString>(ApplicationSettings::chatDbFile));
   if (!chatDb_->loadKeys(pubKeys_)) {
      throw std::runtime_error("failed to load chat public keys");
   }

   heartbeatTimer_.setInterval(30 * 1000);
   heartbeatTimer_.setSingleShot(false);
   connect(&heartbeatTimer_, &QTimer::timeout, this, &ChatClient::sendHeartbeat);
   heartbeatTimer_.start();
}

ChatClient::~ChatClient()
{
   if (loggedIn_) {
      logout();
   }
}

std::string ChatClient::loginToServer(const std::string& email, const std::string& jwt)
{
   if (connection_) {
      logger_->error("[ChatClient::loginToServer] connecting with not purged connection");
      return std::string();
   }

   auto bytesHash = autheid::getSHA256(email.c_str(), email.size());
   currentUserId_ = QString::fromStdString(autheid::base64Encode(bytesHash).substr(0, 8)).toLower().toStdString();

   connection_ = connectionManager_->CreateSecuredDataConnection();
   BinaryData inSrvPubKey(appSettings_->get<std::string>(ApplicationSettings::chatServerPubKey));
   connection_->SetServerPublicKey(inSrvPubKey);
   if (!connection_->openConnection(appSettings_->get<std::string>(ApplicationSettings::chatServerHost)
                            , appSettings_->get<std::string>(ApplicationSettings::chatServerPort), this))
   {
      logger_->error("[ChatClient::loginToServer] failed to open ZMQ data connection");
      connection_.reset();
   }

   auto loginRequest = std::make_shared<Chat::LoginRequest>("", currentUserId_, jwt);
   sendRequest(loginRequest);

   return currentUserId_;
}

void ChatClient::OnLoginReturned(const Chat::LoginResponse &response)
{
   if (response.getStatus() == Chat::LoginResponse::Status::LoginOk) {
      loggedIn_ = true;
      auto request = std::make_shared<Chat::MessagesRequest>("", currentUserId_, currentUserId_);
      sendRequest(request);
   }
   else {
      loggedIn_ = false;
      emit LoginFailed();
   }
}

void ChatClient::logout()
{
   loggedIn_ = false;

   if (!connection_) {
      logger_->error("[ChatClient::logout] Disconnected already");
      return;
   }

   auto request = std::make_shared<Chat::LogoutRequest>("", currentUserId_, "");
   sendRequest(request);

   currentUserId_.clear();
   connection_.reset();
}

void ChatClient::sendRequest(const std::shared_ptr<Chat::Request>& request)
{
   const auto requestData = request->getData();
   logger_->debug("[ChatClient::sendRequest] {}", requestData);

   if (!connection_->isActive()) {
      logger_->error("Connection is not alive!");
   }
   connection_->send(requestData);
}

void ChatClient::sendHeartbeat()
{
   if (loggedIn_ && connection_->isActive()) {
      sendRequest(std::make_shared<Chat::HeartbeatPingRequest>(currentUserId_));
   }
}

void ChatClient::OnHeartbeatPong(const Chat::HeartbeatPongResponse &response)
{
   logger_->debug("[ChatClient::OnHeartbeatPong] {}", response.getData());
}

void ChatClient::OnUsersList(const Chat::UsersListResponse &response)
{
   logger_->debug("Received users list from server: {}", response.getData());
   switch (response.command()) {
   case Chat::UsersListResponse::Command::Replace:
      emit UsersReplace(response.getDataList());
      break;
   case Chat::UsersListResponse::Command::Add:
      emit UsersAdd(response.getDataList());
      break;
   case Chat::UsersListResponse::Command::Delete:
      emit UsersDel(response.getDataList());
      break;
   }
}

void ChatClient::OnMessages(const Chat::MessagesResponse &response)
{
   logger_->debug("Received messages from server: {}", response.getData());
   std::vector<std::shared_ptr<Chat::MessageData>> messages;
   for (const auto &msgStr : response.getDataList()) {
      const auto msg = Chat::MessageData::fromJSON(msgStr);
      chatDb_->add(*msg);

      if (msg->getState() & (int)Chat::MessageData::State::Encrypted) {
         if (!msg->decrypt(ownPrivKey_)) {
            logger_->error("Failed to decrypt msg {}", msg->getId().toStdString());
            msg->setFlag(Chat::MessageData::State::Invalid);
         }
      }
      messages.push_back(msg);
   }

   emit MessagesUpdate(messages);
}

void ChatClient::OnDataReceived(const std::string& data)
{
   logger_->debug("[ChatClient::OnDataReceived] {}", data);

   auto response = Chat::Response::fromJSON(data);
   response->handle(*this);
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

std::shared_ptr<Chat::MessageData> ChatClient::SendOwnMessage(const QString &message, const QString &receiver)
{
   logger_->debug("[ChatClient::sendMessage] {}", message.toStdString());

   Chat::MessageData msg(QString::fromStdString(currentUserId_), receiver
      , QString::fromStdString(CryptoPRNG::generateRandom(8).toHexStr())
      , QDateTime::currentDateTimeUtc(), message);

   auto localEncMsg = msg;
   //TODO: encrypt with own public key
   chatDb_->add(localEncMsg);

   const auto &itPub = pubKeys_.find(receiver);
   if (itPub != pubKeys_.end()) {
      if (!msg.encrypt(itPub->second)) {
         logger_->error("[ChatClient::sendMessage] failed to encrypt message {}"
            , msg.getId().toStdString());
      }
   }

   auto request = std::make_shared<Chat::SendMessageRequest>("", msg.toJsonString());
   sendRequest(request);
   return std::make_shared<Chat::MessageData>(msg);
}

void ChatClient::retrieveUserMessages(const QString &userId)
{
   auto messages = chatDb_->getUserMessages(userId);
   if (!messages.empty()) {
      for (auto &msg : messages) {
         if (msg->getState() & (int)Chat::MessageData::State::Encrypted) {
            if (!msg->decrypt(ownPrivKey_)) {
               logger_->error("Failed to decrypt msg from DB {}", msg->getId().toStdString());
               msg->setFlag(Chat::MessageData::State::Invalid);
            }
         }
      }
      emit MessagesUpdate(messages);
   }
}
