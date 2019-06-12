#include "BaseChatClient.h"

#include "ConnectionManager.h"
#include "UserHasher.h"
#include "Encryption/AEAD_Encryption.h"
#include "Encryption/AEAD_Decryption.h"
#include "Encryption/IES_Encryption.h"
#include "Encryption/IES_Decryption.h"
#include "Encryption/ChatSessionKeyData.h"

BaseChatClient::BaseChatClient(const std::shared_ptr<ConnectionManager>& connectionManager
                               , const std::shared_ptr<spdlog::logger>& logger
                               , const QString& dbFile)
  : logger_{logger}
  , connectionManager_{connectionManager}
{
   chatSessionKeyPtr_ = std::make_shared<Chat::ChatSessionKey>(logger);
   hasher_ = std::make_shared<UserHasher>();

   chatDb_ = make_unique<ChatDB>(logger, dbFile);

   bool loaded = false;
   setSavedKeys(chatDb_->loadKeys(&loaded));

   if (!loaded) {
      logger_->error("[BaseChatClient::BaseChatClient] failed to load saved keys");
   }

   //This is required (with Qt::QueuedConnection), because of ZmqBIP15XDataConnection crashes when delete it from this (callback) thread
   connect(this, &BaseChatClient::CleanupConnection, this, &BaseChatClient::onCleanupConnection, Qt::QueuedConnection);
}

void ChatClient::OnDataReceived(const std::string& data)
{
   auto response = Chat::Response::fromJSON(data);
   if (!response) {
      logger_->error("[ChatClient::OnDataReceived] failed to parse message:\n{}", data);
      return;
   }
   // Process on main thread because otherwise ChatDB could crash
   QMetaObject::invokeMethod(this, [this, response] {
      response->handle(*this);
   });
}

void ChatClient::OnConnected()
{
   logger_->debug("[ChatClient::OnConnected]");
   BinaryData localPublicKey(appSettings_->GetAuthKeys().second.data(), appSettings_->GetAuthKeys().second.size());
   auto loginRequest = std::make_shared<Chat::LoginRequest>("", currentUserId_, currentJwt_, localPublicKey.toHexStr());
   sendRequest(loginRequest);
}

void ChatClient::OnError(DataConnectionError errorCode)
{
   logger_->debug("[ChatClient::OnError] {}", errorCode);
}

std::string BaseChatClient::LoginToServer(const std::string& email, const std::string& jwt
   , const ZmqBIP15XDataConnection::cbNewKey &cb)
{
   if (connection_) {
      logger_->error("[BaseChatClient::LoginToServer] connecting with not purged connection");
      return {};
   }

   currentUserId_ = hasher_->deriveKey(email);
   currentJwt_ = jwt;

   connection_ = connectionManager_->CreateZMQBIP15XDataConnection();
   connection_->setCBs(cb);

   if (!connection_->openConnection( getChatServerHost(), getChatServerPort(), this))
   {
      logger_->error("[BaseChatClient::LoginToServer] failed to open ZMQ data connection");
      connection_.reset();
   }

   return currentUserId_;
}

void BaseChatClient::LogoutFromServer()
{
   if (!connection_) {
      logger_->error("[BaseChatClient::LogoutFromServer] Disconnected already");
      return;
   }

   auto request = std::make_shared<Chat::LogoutRequest>("", currentUserId_, "", "");
   sendRequest(request);

   emit CleanupConnection();
}

void BaseChatClient::onCleanupConnection()
{
   chatSessionKeyPtr_->clearAll();
   currentUserId_.clear();
   currentJwt_.clear();
   connection_.reset();

   OnLogoutCompleted();
}

void ChatClient::OnDisconnected()
{
   logger_->debug("[ChatClient::OnDisconnected]");
   emit CleanupConnection();
}

void ChatClient::OnLoginReturned(const Chat::LoginResponse &response)
{
   if (response.getStatus() == Chat::LoginResponse::Status::LoginOk) {
      OnLoginCompleted();
   }
   else {
      OnLofingFailed();
   }
}

void ChatClient::OnLogoutResponse(const Chat::LogoutResponse & response)
{
   logger_->debug("[ChatClient::OnLogoutResponse]: Server sent logout response with data: {}", response.getData());
   emit CleanupConnection();
}

void BaseChatClient::setSavedKeys(std::map<QString, BinaryData>&& loadedKeys)
{
   std::swap(contactPublicKeys_, loadedKeys);
}

bool BaseChatClient::sendRequest(const std::shared_ptr<Chat::Request>& request)
{
   const auto requestData = request->getData();
   logger_->debug("[BaseChatClient::sendRequest] {}", requestData);

   if (!connection_->isActive()) {
      logger_->error("[BaseChatClient::sendRequest] Connection is not alive!");
      return false;
   }
   return connection_->send(requestData);
}


bool BaseChatClient::sendFriendRequestToServer(const QString &friendUserId)
{
   auto request = std::make_shared<Chat::ContactActionRequestDirect>(
            "",
            currentUserId_,
            friendUserId.toStdString(),
            Chat::ContactsAction::Request,
            getOwnAuthPublicKey());
   return sendRequest(request);
}

bool BaseChatClient::sendAcceptFriendRequestToServer(const QString &friendUserId)
{
   auto requestDirect = std::make_shared<Chat::ContactActionRequestDirect>(
            "", currentUserId_, friendUserId.toStdString(),
            Chat::ContactsAction::Accept,
            getOwnAuthPublicKey());

   sendRequest(requestDirect);

   BinaryData publicKey = contactPublicKeys_[friendUserId];
   auto requestRemote = std::make_shared<Chat::ContactActionRequestServer>(
            "",
            currentUserId_,
            friendUserId.toStdString(),
            Chat::ContactsActionServer::AddContactRecord,
            Chat::ContactStatus::Accepted,
            publicKey);
   sendRequest(requestRemote);

   return true;
}

bool BaseChatClient::sendDeclientFriendRequestToServer(const QString &friendUserId)
{
   auto request = std::make_shared<Chat::ContactActionRequestDirect>(
            "",
            currentUserId_,
            friendUserId.toStdString(),
            Chat::ContactsAction::Reject,
            getOwnAuthPublicKey());
   sendRequest(request);

   BinaryData publicKey = contactPublicKeys_[friendUserId];
   auto requestRemote =
         std::make_shared<Chat::ContactActionRequestServer>(
            "",
            currentUserId_,
            friendUserId.toStdString(),
            Chat::ContactsActionServer::AddContactRecord,
            Chat::ContactStatus::Rejected,
            publicKey);
   sendRequest(requestRemote);

   return true;
}

bool BaseChatClient::sendUpdateMessageState(const std::shared_ptr<Chat::MessageData>& message)
{
   auto request = std::make_shared<Chat::MessageChangeStatusRequest>(
            currentUserId_, message->id().toStdString(), message->state());

   return sendRequest(request);
}

bool BaseChatClient::sendSearchUsersRequest(const QString &userIdPattern)
{
   auto request = std::make_shared<Chat::SearchUsersRequest>(
                     "",
                     currentUserId_,
                     userIdPattern.toStdString());

   return sendRequest(request);
}

QString BaseChatClient::deriveKey(const QString &email) const
{
   return QString::fromStdString(hasher_->deriveKey(email.toStdString()));
}


QString BaseChatClient::getUserId() const
{
   return QString::fromStdString(currentUserId_);
}

bool BaseChatClient::decodeAndUpdateIncomingSessionPublicKey(const std::string& senderId, const std::string& encodedPublicKey)
{
   BinaryData test(appSettings_->GetAuthKeys().second.data(), appSettings_->GetAuthKeys().second.size());

   // decrypt by ies received public key
   std::unique_ptr<Encryption::IES_Decryption> dec = Encryption::IES_Decryption::create(logger_);
   dec->setPrivateKey(getOwnAuthPrivateKey());

   std::string encryptedData = QByteArray::fromBase64(QString::fromStdString(encodedPublicKey).toLatin1()).toStdString();

   dec->setData(encryptedData);

   Botan::SecureVector<uint8_t> decodedData;
   try {
      dec->finish(decodedData);
   }
   catch (std::exception&) {
      logger_->error("[ChatClient::{}] Failed to decrypt public key by ies.", __func__);
      return false;
   }

   BinaryData remoteSessionPublicKey = BinaryData::CreateFromHex(QString::fromUtf8((char*)decodedData.data(), (int)decodedData.size()).toStdString());

   chatSessionKeyPtr_->generateLocalKeysForUser(senderId);
   chatSessionKeyPtr_->updateRemotePublicKeyForUser(senderId, remoteSessionPublicKey);

   return true;
}
