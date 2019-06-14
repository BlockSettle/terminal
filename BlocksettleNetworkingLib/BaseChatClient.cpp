#include "BaseChatClient.h"

#include "ConnectionManager.h"
#include "UserHasher.h"
#include "Encryption/AEAD_Encryption.h"
#include "Encryption/AEAD_Decryption.h"
#include "Encryption/IES_Encryption.h"
#include "Encryption/IES_Decryption.h"
#include "Encryption/ChatSessionKeyData.h"

#include <disable_warnings.h>
#include <botan/bigint.h>
#include <botan/base64.h>
#include <botan/auto_rng.h>
#include <enable_warnings.h>

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

BaseChatClient::~BaseChatClient() noexcept
{}

void BaseChatClient::OnDataReceived(const std::string& data)
{
   auto response = Chat::Response::fromJSON(data);
   if (!response) {
      logger_->error("[BaseChatClient::OnDataReceived] failed to parse message:\n{}", data);
      return;
   }
   // Process on main thread because otherwise ChatDB could crash
   QMetaObject::invokeMethod(this, [this, response] {
      response->handle(*this);
   });
}

void BaseChatClient::OnConnected()
{
   logger_->debug("[BaseChatClient::OnConnected]");
   BinaryData localPublicKey(getOwnAuthPublicKey());
   auto loginRequest = std::make_shared<Chat::LoginRequest>("", currentUserId_, currentJwt_, localPublicKey.toHexStr());
   sendRequest(loginRequest);
}

void BaseChatClient::OnError(DataConnectionError errorCode)
{
   logger_->debug("[BaseChatClient::OnError] {}", errorCode);
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

void BaseChatClient::OnDisconnected()
{
   logger_->debug("[BaseChatClient::OnDisconnected]");
   emit CleanupConnection();
}

void BaseChatClient::OnLoginReturned(const Chat::LoginResponse &response)
{
   if (response.getStatus() == Chat::LoginResponse::Status::LoginOk) {
      OnLoginCompleted();
   }
   else {
      OnLofingFailed();
   }
}

void BaseChatClient::OnLogoutResponse(const Chat::LogoutResponse & response)
{
   logger_->debug("[BaseChatClient::OnLogoutResponse]: Server sent logout response with data: {}", response.getData());
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
   BinaryData test(getOwnAuthPublicKey());

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
      logger_->error("[BaseChatClient::{}] Failed to decrypt public key by ies.", __func__);
      return false;
   }

   BinaryData remoteSessionPublicKey = BinaryData::CreateFromHex(QString::fromUtf8((char*)decodedData.data(), (int)decodedData.size()).toStdString());

   chatSessionKeyPtr_->generateLocalKeysForUser(senderId);
   chatSessionKeyPtr_->updateRemotePublicKeyForUser(senderId, remoteSessionPublicKey);

   return true;
}

void BaseChatClient::OnSendMessageResponse(const Chat::SendMessageResponse& response)
{
   QJsonDocument json(response.toJson());
   logger_->debug("[BaseChatClient::OnSendMessageResponse]: received: {}",
                  json.toJson(QJsonDocument::Indented).toStdString());

   if (response.getResult() == Chat::SendMessageResponse::Result::Accepted) {
      QString localId = QString::fromStdString(response.clientMessageId());
      QString serverId = QString::fromStdString(response.serverMessageId());
      QString receiverId = QString::fromStdString(response.receiverId());

      if (!chatDb_->syncMessageId(localId, serverId)) {
         logger_->error("[BaseChatClient::OnSendMessageResponse] failed to update message id in DB from {} to {}"
                        , localId.toStdString(), serverId.toStdString());
      }

      onMessageSent(receiverId, localId, serverId);
   }
}

void BaseChatClient::OnMessageChangeStatusResponse(const Chat::MessageChangeStatusResponse& response)
{
   auto messageId = QString::fromStdString(response.messageId());
   int newStatus = response.getUpdatedStatus();

   if (chatDb_->updateMessageStatus(messageId, newStatus)) {
      QString chatId = QString::fromStdString(response.messageSenderId() == currentUserId_
                    ? response.messageReceiverId()
                    : response.messageSenderId());

      onMessageStatusChanged(chatId, messageId, newStatus);
   } else {
      logger_->error("[BaseChatClient::OnMessageChangeStatusResponse] failed to update message state in DB: {} {}"
                     , response.messageId(), newStatus);
   }
}

void BaseChatClient::OnContactsActionResponseDirect(const Chat::ContactsActionResponseDirect& response)
{
   std::string actionString = "<unknown>";
   switch (response.getAction()) {
      case Chat::ContactsAction::Accept: {
         actionString = "ContactsAction::Accept";
         QString senderId = QString::fromStdString(response.senderId());
         contactPublicKeys_[senderId] = response.getSenderPublicKey();
         chatDb_->addKey(senderId, response.getSenderPublicKey());

         onContactAccepted(senderId);

         addOrUpdateContact(senderId, Chat::ContactStatus::Accepted);
         auto requestS = std::make_shared<Chat::ContactActionRequestServer>(""
                  , currentUserId_
                  , senderId.toStdString()
                  , Chat::ContactsActionServer::UpdateContactRecord
                  , Chat::ContactStatus::Accepted, response.getSenderPublicKey());
         sendRequest(requestS);
         // reprocess message again
         retrySendQueuedMessages(response.senderId());
      }
      break;
      case Chat::ContactsAction::Reject: {
         actionString = "ContactsAction::Reject";
         addOrUpdateContact(QString::fromStdString(response.senderId()), Chat::ContactStatus::Rejected);
         onContactRejected(QString::fromStdString(response.senderId()));

         auto requestS = std::make_shared<Chat::ContactActionRequestServer>(""
                  , currentUserId_
                  , response.senderId()
                  , Chat::ContactsActionServer::UpdateContactRecord
                  , Chat::ContactStatus::Rejected, response.getSenderPublicKey());
         sendRequest(requestS);
         //removeContact(QString::fromStdString(response.senderId()));
         eraseQueuedMessages(response.senderId());
      }
      break;
      case Chat::ContactsAction::Request: {
         actionString = "ContactsAction::Request";
         QString userId = QString::fromStdString(response.receiverId());
         QString contactId = QString::fromStdString(response.senderId());
         BinaryData pk = response.getSenderPublicKey();
         contactPublicKeys_[contactId] = response.getSenderPublicKey();
         chatDb_->addKey(contactId, response.getSenderPublicKey());

         onFriendRequest(userId, QString::fromStdString(response.senderId()), pk);
         //addOrUpdateContact(QString::fromStdString(response.senderId()), QStringLiteral(""), true);
      }
      break;
      case Chat::ContactsAction::Remove: {
         BinaryData pk = response.getSenderPublicKey();
         auto requestS = std::make_shared<Chat::ContactActionRequestServer>(""
                  , currentUserId_
                  , response.senderId()
                  , Chat::ContactsActionServer::RemoveContactRecord
                  , Chat::ContactStatus::Incoming, pk);

         sendRequest(requestS);
      }
         break;

   }
   logger_->debug("[BaseChatClient::OnContactsActionResponseDirect]: Incoming contact action from {}: {}"
                  , response.senderId(), actionString);
}

void BaseChatClient::OnContactsActionResponseServer(const Chat::ContactsActionResponseServer & response)
{
   std::string actionString = "<unknown>";
   switch (response.getRequestedAction()) {
      case Chat::ContactsActionServer::AddContactRecord:
         actionString = "ContactsActionServer::AddContactRecord";
         //addOrUpdateContact(QString::fromStdString(response.userId()));
         retrySendQueuedMessages(response.contactId());
      break;
      case Chat::ContactsActionServer::RemoveContactRecord:
         actionString = "ContactsActionServer::RemoveContactRecord";
         //removeContact(QString::fromStdString(response.userId()));
         if (response.getActionResult() == Chat::ContactsActionServerResult::Success) {
            onContactRemove(QString::fromStdString(response.contactId()));
            chatDb_->removeContact(QString::fromStdString(response.contactId()));
            //TODO: Remove pub key
         }
         eraseQueuedMessages(response.contactId());
      break;
      case Chat::ContactsActionServer::UpdateContactRecord:
         actionString = "ContactsActionServer::UpdateContactRecord";
         //addOrUpdateContact(QString::fromStdString(response.userId()), QStringLiteral(""), true);
      break;
      default:
      break;
   }

   std::string actionResString = "<unknown>";
   switch (response.getActionResult()) {
      case Chat::ContactsActionServerResult::Success:
         actionResString = "ContactsActionServerResult::Success";
      break;
      case Chat::ContactsActionServerResult::Failed:
         actionResString = "ContactsActionServerResult::Failed";
      break;
      default:
      break;
   }
}

void BaseChatClient::OnContactsListResponse(const Chat::ContactsListResponse & response)
{
   for (const auto& contact : response.getContactsList()) {
      contactPublicKeys_[contact->getContactId()] = contact->getContactPublicKey();
   }

   onContactListLoaded(response.getContactsList());
}

void BaseChatClient::OnChatroomsList(const Chat::ChatroomsListResponse& response)
{
   for (const auto& room : response.getChatRoomList()) {
      chatDb_->removeRoomMessages(room->getId());
   }

   onRoomsLoaded(response.getChatRoomList());
}

void BaseChatClient::OnRoomMessages(const Chat::RoomMessagesResponse& response)
{
   logger_->debug("[BaseChatClient::OnRoomMessages] Received chatroom messages from server (receiver id is chatroom): {}"
                  , response.getData());

   for (const auto &msgStr : response.getDataList()) {
      const auto msg = Chat::MessageData::fromJSON(msgStr);
      msg->setFlag(Chat::MessageData::State::Acknowledged);
      /*chatDb_->add(*msg);

      if (msg->encryptionType() == Chat::MessageData::EncryptionType::IES) {
         if (!msg->decrypt(ownPrivKey_)) {
            logger_->error("Failed to decrypt msg {}", msg->getId().toStdString());
            msg->setFlag(Chat::MessageData::State::Invalid);
         }
         else {
            msg->setEncryptionType(Chat::MessageData::EncryptionType::Unencrypted);
         }
      }*/
      onRoomMessageReceived(msg);
   }
}

void BaseChatClient::OnSearchUsersResponse(const Chat::SearchUsersResponse & response)
{
   onSearchResult(response.getUsersList());
}

void BaseChatClient::OnUsersList(const Chat::UsersListResponse& response)
{
   for (const auto& user : response.getDataList()) {
      // if status changed clear session keys for contact
      chatSessionKeyPtr_->clearSessionForUser(user);
   }

   onUserListChanged(response.command(), response.getDataList());
}

void BaseChatClient::OnMessages(const Chat::MessagesResponse &response)
{
   logger_->debug("[BaseChatClient::OnMessages] Received messages from server: {}"
                  , response.getData());

   std::vector<std::shared_ptr<Chat::MessageData>> messages;
   for (const auto &msgStr : response.getDataList()) {
      auto msg = Chat::MessageData::fromJSON(msgStr);
      msg->setMessageDirection(Chat::MessageData::MessageDirection::Received);

      if (!chatDb_->isContactExist(msg->senderId())) {
         continue;
      }

      msg->setFlag(Chat::MessageData::State::Acknowledged);

      switch (msg->encryptionType()) {
         case Chat::MessageData::EncryptionType::AEAD:
         {
            std::string senderId = msg->senderId().toStdString();
            const auto& chatSessionKeyDataPtr = chatSessionKeyPtr_->findSessionForUser(senderId);

            if (!chatSessionKeyPtr_->isExchangeForUserSucceeded(senderId)) {
               logger_->error("[BaseChatClient::OnMessages] Can't find public key for sender {}"
                  , msg->senderId().toStdString());
               msg->setFlag(Chat::MessageData::State::Invalid);
            }
            else {
               BinaryData remotePublicKey(chatSessionKeyDataPtr->remotePublicKey());
               SecureBinaryData localPrivateKey(chatSessionKeyDataPtr->localPrivateKey());

               std::unique_ptr<Encryption::AEAD_Decryption> dec = Encryption::AEAD_Decryption::create(logger_);
               dec->setPrivateKey(localPrivateKey);
               dec->setPublicKey(remotePublicKey);
               dec->setNonce(msg->nonce());
               dec->setData(QByteArray::fromBase64(msg->messagePayload().toLatin1()).toStdString());
               dec->setAssociatedData(msg->jsonAssociatedData());

               try {
                  Botan::SecureVector<uint8_t> decodedData;
                  dec->finish(decodedData);

                  // create new instance of decrypted message
                  msg = msg->CreateDecryptedMessage(QString::fromUtf8((char*)decodedData.data(),(int)decodedData.size()));
               }
               catch (std::exception & e) {
                  logger_->error("[BaseChatClient::OnMessages] Failed to decrypt aead msg {}", e.what());
                  msg->setFlag(Chat::MessageData::State::Invalid);
               }
            }

            onDMMessageReceived(msg);

            encryptByIESAndSaveMessageInDb(msg);
         }
         break;

         case Chat::MessageData::EncryptionType::IES:
         {
            logger_->error("[BaseChatClient::OnMessages] This could not happen! Failed to decrypt msg.");
            onDMMessageReceived(msg);
            break;
         }

         default:
            break;
      }
      sendUpdateMessageState(msg);
   }
}

void BaseChatClient::OnAskForPublicKey(const Chat::AskForPublicKeyResponse &response)
{
   logger_->debug("Received request to send own public key from server: {}", response.getData());

   // Make sure we are the node for which a public key was expected, if not, ignore this call.
   if ( currentUserId_ != response.getPeerId()) {
      return;
   }

   // Send our key to the peer.
   auto request = std::make_shared<Chat::SendOwnPublicKeyRequest>(
      "", // clientId
      response.getAskingNodeId(),
      response.getPeerId(),
      BinaryData(getOwnAuthPublicKey()));
   sendRequest(request);
}

void BaseChatClient::OnSendOwnPublicKey(const Chat::SendOwnPublicKeyResponse &response)
{
   logger_->debug("[BaseChatClient::OnSendOwnPublicKey] Received public key of peer from server: {}", response.getData());

   // Make sure we are the node for which a public key was expected, if not, ignore this call.
   if ( currentUserId_ != response.getReceivingNodeId()) {
      return;
   }
   // Save received public key of peer.
   const auto peerId = QString::fromStdString(response.getSendingNodeId());
   contactPublicKeys_[peerId] = response.getSendingNodePublicKey();
   chatDb_->addKey(peerId, response.getSendingNodePublicKey());

   // Run over enqueued messages if any, and try to send them all now.
   messages_queue& messages = enqueued_messages_[QString::fromStdString(response.getSendingNodeId())];
   while (!messages.empty()) {
      sendMessageDataRequest(messages.front(), QString::fromStdString(response.getSendingNodeId()));
      messages.pop();
   }
}

bool BaseChatClient::getContacts(ContactRecordDataList &contactList)
{
   return chatDb_->getContacts(contactList);
}

bool BaseChatClient::addOrUpdateContact(const QString &userId, Chat::ContactStatus status, const QString &userName)
{
   Chat::ContactRecordData contact(userId, userId, status, BinaryData(), userName);

   if (chatDb_->isContactExist(userId))
   {
      return chatDb_->updateContact(contact);
   }

   return chatDb_->addContact(contact);
}

bool BaseChatClient::removeContact(const QString &userId)
{
   return chatDb_->removeContact(userId);
}

void BaseChatClient::OnSessionPublicKeyResponse(const Chat::SessionPublicKeyResponse& response)
{
   if (!decodeAndUpdateIncomingSessionPublicKey(response.senderId(), response.senderSessionPublicKey())) {
      logger_->error("[BaseChatClient::OnSessionPublicKeyResponse] Failed updating remote public key!");
      return;
   }

   // encode own session public key by ies and send as reply
   const auto& contactPublicKeyIterator = contactPublicKeys_.find(QString::fromStdString(response.senderId()));
   if (contactPublicKeyIterator == contactPublicKeys_.end()) {
      // this should not happen
      logger_->error("[BaseChatClient::OnSessionPublicKeyResponse] Cannot find remote public key!");
      return;
   }

   BinaryData remotePublicKey(contactPublicKeyIterator->second);

   try {
      BinaryData encryptedLocalPublicKey = chatSessionKeyPtr_->iesEncryptLocalPublicKey(response.senderId(), remotePublicKey);

      std::string encryptedString = QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(encryptedLocalPublicKey.getPtr()),
         int(encryptedLocalPublicKey.getSize())).toBase64()).toStdString();

      auto request = std::make_shared<Chat::ReplySessionPublicKeyRequest>(
         "",
         currentUserId_,
         response.senderId(),
         encryptedString);

      sendRequest(request);
   }
   catch (std::exception& e) {
      logger_->error("[BaseChatClient::OnSessionPublicKeyResponse] Failed to encrypt msg by ies {}", e.what());
      return;
   }
}

void BaseChatClient::OnReplySessionPublicKeyResponse(const Chat::ReplySessionPublicKeyResponse& response)
{
   if (!decodeAndUpdateIncomingSessionPublicKey(response.senderId(), response.senderSessionPublicKey())) {
      logger_->error("[BaseChatClient::OnReplySessionPublicKeyResponse] Failed updating remote public key!");
      return;
   }

   // Run over enqueued messages if any, and try to send them all now.
   messages_queue messages = enqueued_messages_[QString::fromStdString(response.senderId())];

   while (!messages.empty()) {
      sendMessageDataRequest(messages.front(), QString::fromStdString(response.senderId()));
      messages.pop();
   }
}

std::shared_ptr<Chat::MessageData> BaseChatClient::sendMessageDataRequest(const std::shared_ptr<Chat::MessageData>& messageData
                                                                      , const QString &receiver)
{
   messageData->setMessageDirection(Chat::MessageData::MessageDirection::Sent);

   if (!chatDb_->isContactExist(receiver)) {
      //make friend request before sending direct message.
      //Enqueue the message to be sent, once our friend request accepted.
      enqueued_messages_[receiver].push(messageData);
      // we should not send friend request from here. this is user action
      // sendFriendRequest(receiver);
      return messageData;
   } else {
      // is contact rejected?
      Chat::ContactRecordData contact(QString(),
                                      QString(),
                                      Chat::ContactStatus::Accepted,
                                      BinaryData());
      chatDb_->getContact(messageData->receiverId(), contact);

      if (contact.getContactStatus() == Chat::ContactStatus::Rejected)
      {
         logger_->error("[BaseChatClient::sendMessageDataRequest] {}",
                        "Receiver has rejected state. Discarding message."
                        , receiver.toStdString());
         messageData->setFlag(Chat::MessageData::State::Invalid);
         return messageData;
      }
   }

   const auto &contactPublicKeyIterator = contactPublicKeys_.find(receiver);
   if (contactPublicKeyIterator == contactPublicKeys_.end()) {
      // Ask for public key from peer. Enqueue the message to be sent, once we receive the
      // necessary public key.
      enqueued_messages_[receiver].push(messageData);

      // Send our key to the peer.
      auto request = std::make_shared<Chat::AskForPublicKeyRequest>(
         "", // clientId
         currentUserId_,
         receiver.toStdString());
      sendRequest(request);
      return messageData;
   }

   const auto& chatSessionKeyDataPtr = chatSessionKeyPtr_->findSessionForUser(receiver.toStdString());
   if (chatSessionKeyDataPtr == nullptr || !chatSessionKeyPtr_->isExchangeForUserSucceeded(receiver.toStdString())) {
      enqueued_messages_[receiver].push(messageData);

      chatSessionKeyPtr_->generateLocalKeysForUser(receiver.toStdString());

      BinaryData remotePublicKey(contactPublicKeyIterator->second);
      logger_->debug("[BaseChatClient::sendMessageDataRequest] USING PUBLIC KEY: {}", remotePublicKey.toHexStr());

      try {
         BinaryData encryptedLocalPublicKey = chatSessionKeyPtr_->iesEncryptLocalPublicKey(receiver.toStdString(), remotePublicKey);

         std::string encryptedString = QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(encryptedLocalPublicKey.getPtr()),
               int(encryptedLocalPublicKey.getSize())).toBase64()).toStdString();

         auto request = std::make_shared<Chat::SessionPublicKeyRequest>(
            "",
            currentUserId_,
            receiver.toStdString(),
            encryptedString);

         sendRequest(request);
         return messageData;
      } catch (std::exception& e) {
         logger_->error("[BaseChatClient::sendMessageDataRequest] Failed to encrypt msg by ies {}", e.what());
         return messageData;
      }
   }

   if (!encryptByIESAndSaveMessageInDb(messageData))
   {
      logger_->error("[BaseChatClient::sendMessageDataRequest] failed to encrypt. discarding message");
      messageData->setFlag(Chat::MessageData::State::Invalid);
      return messageData;
   }

   onDMMessageReceived(messageData);

   // search active message session for given user
   const auto userNoncesIterator = userNonces_.find(receiver);
   Botan::SecureVector<uint8_t> nonce;
   if (userNoncesIterator == userNonces_.end()) {
      // generate random nonce
      Botan::AutoSeeded_RNG rng;
      nonce = rng.random_vec(messageData->defaultNonceSize());
      userNonces_.emplace_hint(userNoncesIterator, receiver, nonce);
   }
   else {
      // read nonce and increment
      Botan::BigInt bigIntNonce;
      bigIntNonce.binary_decode(userNoncesIterator->second);
      bigIntNonce++;
      nonce = Botan::BigInt::encode_locked(bigIntNonce);
      userNoncesIterator->second = nonce;
   }

   std::unique_ptr<Encryption::AEAD_Encryption> enc = Encryption::AEAD_Encryption::create(logger_);

   enc->setPrivateKey(chatSessionKeyDataPtr->localPrivateKey());
   enc->setPublicKey(chatSessionKeyDataPtr->remotePublicKey());

   enc->setNonce(nonce);
   messageData->setNonce(nonce);

   enc->setData(messageData->messagePayload().toStdString());
   enc->setAssociatedData(messageData->jsonAssociatedData());

   Botan::SecureVector<uint8_t> encodedData;

   try {
      enc->finish(encodedData);
   }
   catch (std::exception & e) {
      logger_->error("[BaseChatClient::sendMessageDataRequest] Can't encode data {}", e.what());
      messageData->setFlag(Chat::MessageData::State::Invalid);
      return messageData;
   }

   auto encryptedMessage = messageData->CreateEncryptedMessage(Chat::MessageData::EncryptionType::AEAD
      , QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(encodedData.data()), int(encodedData.size())).toBase64()));

   auto request = std::make_shared<Chat::SendMessageRequest>("", encryptedMessage->toJsonString());
   sendRequest(request);

   return messageData;
}

void BaseChatClient::retrySendQueuedMessages(const std::string userId)
{
   // Run over enqueued messages if any, and try to send them all now.
   messages_queue& messages = enqueued_messages_[QString::fromStdString(userId)];

   while (!messages.empty()) {
      sendMessageDataRequest(messages.front(), QString::fromStdString(userId));
      messages.pop();
   }
}

void BaseChatClient::eraseQueuedMessages(const std::string userId)
{
   enqueued_messages_.erase(QString::fromStdString(userId));
}

bool BaseChatClient::encryptByIESAndSaveMessageInDb(const std::shared_ptr<Chat::MessageData>& message)
{
   BinaryData localPublicKey(getOwnAuthPublicKey());
   std::unique_ptr<Encryption::IES_Encryption> enc = Encryption::IES_Encryption::create(logger_);
   enc->setPublicKey(localPublicKey);
   enc->setData(message->messagePayload().toStdString());

   try {
      Botan::SecureVector<uint8_t> encodedData;
      enc->finish(encodedData);

      auto encryptedMessage = message->CreateEncryptedMessage(Chat::MessageData::EncryptionType::IES
                                                              , QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(encodedData.data()), int(encodedData.size())).toBase64()));
      chatDb_->add(encryptedMessage);
   }
   catch (std::exception & e) {
      logger_->error("[BaseChatClient::encryptByIESAndSaveMessageInDb] Failed to encrypt msg by ies {}", e.what());
      return false;
   }

   return true;
}

std::shared_ptr<Chat::MessageData> BaseChatClient::decryptIESMessage(const std::shared_ptr<Chat::MessageData>& message)
{
   std::unique_ptr<Encryption::IES_Decryption> dec = Encryption::IES_Decryption::create(logger_);
   SecureBinaryData localPrivateKey(getOwnAuthPrivateKey());
   dec->setPrivateKey(localPrivateKey);
   dec->setData(QByteArray::fromBase64(message->messagePayload().toUtf8()).toStdString());

   try {
      Botan::SecureVector<uint8_t> decodedData;
      dec->finish(decodedData);

      return message->CreateDecryptedMessage(QString::fromUtf8((char*)decodedData.data(), (int)decodedData.size()));
   }
   catch (std::exception &) {
      logger_->error("[BaseChatClient::decryptIESMessage] Failed to decrypt msg from DB {}", message->id().toStdString());
      message->setFlag(Chat::MessageData::State::Invalid);
      return message;
   }
}
