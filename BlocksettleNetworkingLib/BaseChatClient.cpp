#include "BaseChatClient.h"

#include "ConnectionManager.h"
#include "UserHasher.h"
#include "Encryption/AEAD_Encryption.h"
#include "Encryption/AEAD_Decryption.h"
#include "Encryption/IES_Encryption.h"
#include "Encryption/IES_Decryption.h"
#include "Encryption/ChatSessionKeyData.h"

#include "ProtobufUtils.h"
#include <disable_warnings.h>
#include <botan/bigint.h>
#include <botan/base64.h>
#include <botan/auto_rng.h>
#include <enable_warnings.h>

#include <QDateTime>

#include "ChatProtocol/ChatUtils.h"

BaseChatClient::BaseChatClient(const std::shared_ptr<ConnectionManager>& connectionManager, 
   const std::shared_ptr<spdlog::logger>& logger, const QString& dbFile) 
   : logger_{logger}, connectionManager_{connectionManager}
{
   chatSessionKeyPtr_ = std::make_shared<Chat::ChatSessionKey>(logger);
   contactPublicKeysPtr_ = std::make_shared<Chat::ContactPublicKey>(logger);
   hasher_ = std::make_shared<UserHasher>();

   chatDb_ = make_unique<ChatDB>(logger, dbFile);

   bool loaded = false;
   setSavedKeys(chatDb_->loadKeys(&loaded));

   if (!loaded) {
      logger_->error("[BaseChatClient::{}] failed to load saved keys", __func__);
   }
}

BaseChatClient::~BaseChatClient() noexcept
{}

void BaseChatClient::OnDataReceived(const std::string& data)
{
   auto response = std::make_shared<Chat::Response>();
   bool result = response->ParseFromString(data);

   if (!result) {
      logger_->error("[BaseChatClient::{}] failed to parse message:\n{}", __func__, data);
      return;
   }

   logger_->debug("[BaseChatClient::{}] recv: \n{}", __func__, ProtobufUtils::toJson(*response));

   // Process on main thread because otherwise ChatDB could crash
   QMetaObject::invokeMethod(this, [this, response] {
      switch (response->data_case()) {
         case Chat::Response::kUsersList:
            OnUsersList(response->users_list());
            break;
         case Chat::Response::kMessages:
            OnMessages(response->messages());
            break;
         case Chat::Response::kAskForPublicKey:
            OnAskForPublicKey(response->ask_for_public_key());
            break;
         case Chat::Response::kSendOwnPublicKey:
            OnSendOwnPublicKey(response->send_own_public_key());
            break;
         case Chat::Response::kLogin:
            OnLoginReturned(response->login());
            break;
         case Chat::Response::kLogout:
            OnLogoutResponse(response->logout());
            break;
         case Chat::Response::kSendMessage:
            OnSendMessageResponse(response->send_message());
            break;
         case Chat::Response::kMessageChangeStatus:
            OnMessageChangeStatusResponse(response->message_change_status());
            break;
         case Chat::Response::kModifyContactsDirect:
            OnModifyContactsDirectResponse(response->modify_contacts_direct());
            break;
         case Chat::Response::kModifyContactsServer:
            OnModifyContactsServerResponse(response->modify_contacts_server());
            break;
         case Chat::Response::kContactsList:
            OnContactsListResponse(response->contacts_list());
            break;
         case Chat::Response::kChatroomsList:
            OnChatroomsList(response->chatrooms_list());
            break;
         case Chat::Response::kRoomMessages:
            OnRoomMessages(response->room_messages());
            break;
         case Chat::Response::kSearchUsers:
            OnSearchUsersResponse(response->search_users());
            break;
         case Chat::Response::kSessionPublicKey:
            OnSessionPublicKeyResponse(response->session_public_key());
            break;
         case Chat::Response::kReplySessionPublicKey:
            OnReplySessionPublicKeyResponse(response->reply_session_public_key());
            break;
         case Chat::Response::kConfirmReplacePublicKey:
            OnConfirmReplacePublicKey(response->confirm_replace_public_key());
            break;
         case Chat::Response::DATA_NOT_SET:
            logger_->error("[BaseChatClient::{}] Invalid empty or unknown response detected", __func__);
            break;
      }
   });
}

void BaseChatClient::OnConnected()
{
   logger_->debug("[BaseChatClient::{}]", __func__);

   Chat::Request request;
   auto d = request.mutable_login();
   d->set_auth_id(currentUserId_);
   d->set_jwt(currentJwt_);
   d->set_public_key(getOwnAuthPublicKey().toBinStr());
   sendRequest(request);
}

void BaseChatClient::OnError(DataConnectionError errorCode)
{
   logger_->debug("[BaseChatClient::{}] {}", __func__, errorCode);
}

std::string BaseChatClient::LoginToServer(const std::string& email, const std::string& jwt, const ZmqBIP15XDataConnection::cbNewKey &cb)
{
   if (connection_) {
      logger_->error("[BaseChatClient::{}] connecting with not purged connection", __func__);
      return {};
   }

   currentUserId_ = hasher_->deriveKey(email);
   currentJwt_ = jwt;

   connection_ = connectionManager_->CreateZMQBIP15XDataConnection();
   connection_->setCBs(cb);

   if (!connection_->openConnection( getChatServerHost(), getChatServerPort(), this))
   {
      logger_->error("[BaseChatClient::{}] failed to open ZMQ data connection", __func__);
      connection_.reset();
   }

   return currentUserId_;
}

void BaseChatClient::LogoutFromServer()
{
   if (!connection_) {
      logger_->error("[BaseChatClient::{}] Disconnected already", __func__);
      return;
   }

   Chat::Request request;
   auto d = request.mutable_logout();
   d->set_auth_id(currentUserId_);
   sendRequest(request);

   cleanupConnection();
}

void BaseChatClient::OnDisconnected()
{
   logger_->debug("[BaseChatClient::{}]", __func__);
   QMetaObject::invokeMethod(this, [this] {
      cleanupConnection();
   });
}

void BaseChatClient::OnLoginReturned(const Chat::Response_Login &response)
{
   if (response.success()) {
      OnLoginCompleted();
   }
   else {
      OnLogingFailed();
   }
}

void BaseChatClient::OnLogoutResponse(const Chat::Response_Logout & response)
{
   logger_->debug("[BaseChatClient::{}]", __func__);
   QMetaObject::invokeMethod(this, [this] {
      cleanupConnection();
   });
}

void BaseChatClient::setSavedKeys(std::map<std::string, BinaryData>&& loadedKeys)
{
   contactPublicKeysPtr_->loadKeys(loadedKeys);
}

void BaseChatClient::onCreateOutgoingContact(const std::string &contactId)
{
   addOrUpdateContact(contactId, Chat::CONTACT_STATUS_OUTGOING);
}

bool BaseChatClient::sendRequest(const Chat::Request& request)
{
   logger_->debug("[BaseChatClient::{}] send: \n{}", __func__, ProtobufUtils::toJson(request));

   if (!connection_->isActive()) {
      logger_->error("[BaseChatClient::{}] Connection is not alive!", __func__);
      return false;
   }
   return connection_->send(request.SerializeAsString());
}


bool BaseChatClient::sendFriendRequestToServer(const std::string &friendUserId)
{
   return sendFriendRequestToServer(friendUserId, nullptr);
}

bool BaseChatClient::sendFriendRequestToServer(const std::string &friendUserId, ChatDataPtr message, bool isFromPendings)
{
   if (message) {

      message->set_direction(Chat::Data_Direction_SENT);

      if (!isFromPendings) {
         onCreateOutgoingContact(friendUserId);
         encryptByIESAndSaveMessageInDb(message);
         onCRMessageReceived(message);
      }

      BinaryData contactPublicKey;
      if (!contactPublicKeysPtr_->findPublicKeyForUser(friendUserId, contactPublicKey)) {
         // Ask for public key from peer. Enqueue the message to be sent, once we receive the
         // necessary public key.
         pendingContactRequests_.insert({ friendUserId, message });

         // Send our key to the peer.
         Chat::Request request;
         auto d = request.mutable_ask_for_public_key();
         d->set_asking_node_id(currentUserId_);
         d->set_peer_id(friendUserId);
         return sendRequest(request);
      }

      auto msgEncrypted = encryptMessageToSendIES(contactPublicKey, message);
      Chat::Request request;
      auto d = request.mutable_modify_contacts_direct();
      d->set_sender_id(currentUserId_);
      d->set_receiver_id(friendUserId);
      d->set_action(Chat::CONTACTS_ACTION_REQUEST);
      d->set_sender_pub_key(getOwnAuthPublicKey().toBinStr());
      *d->mutable_message() = std::move(*msgEncrypted);
      return sendRequest(request);
   }

   Chat::Request request;
   auto d = request.mutable_modify_contacts_direct();
   d->set_sender_id(currentUserId_);
   d->set_receiver_id(friendUserId);
   d->set_action(Chat::CONTACTS_ACTION_REQUEST);
   d->set_sender_pub_key(getOwnAuthPublicKey().toBinStr());

   if (sendRequest(request)) {
      onCreateOutgoingContact(friendUserId);
      return true;
   }
   return false;
}

bool BaseChatClient::sendAcceptFriendRequestToServer(const std::string &friendUserId)
{
   {
      Chat::Request request;
      auto d = request.mutable_modify_contacts_direct();
      d->set_sender_id(currentUserId_);
      d->set_receiver_id(friendUserId);
      d->set_action(Chat::CONTACTS_ACTION_ACCEPT);
      sendRequest(request);
   }

   {
      Chat::Request request;
      auto d = request.mutable_modify_contacts_server();
      d->set_sender_id(currentUserId_);
      d->set_contact_id(friendUserId);
      d->set_action(Chat::CONTACTS_ACTION_SERVER_ADD);
      d->set_status(Chat::CONTACT_STATUS_ACCEPTED);
      sendRequest(request);
   }

   return true;
}

bool BaseChatClient::sendRejectFriendRequestToServer(const std::string &friendUserId)
{
   {
      Chat::Request request;
      auto d = request.mutable_modify_contacts_direct();
      d->set_sender_id(currentUserId_);
      d->set_receiver_id(friendUserId);
      d->set_action(Chat::CONTACTS_ACTION_REJECT);
      sendRequest(request);
   }

   {
      Chat::Request request;
      auto d = request.mutable_modify_contacts_server();
      d->set_sender_id(currentUserId_);
      d->set_contact_id(friendUserId);
      d->set_action(Chat::CONTACTS_ACTION_SERVER_ADD);
      d->set_status(Chat::CONTACT_STATUS_REJECTED);
      sendRequest(request);
   }

   return true;
}

bool BaseChatClient::sendRemoveFriendToServer(const std::string &contactId)
{
   Chat::Request request;
   auto d = request.mutable_modify_contacts_server();
   d->set_sender_id(currentUserId_);
   d->set_contact_id(contactId);
   d->set_action(Chat::CONTACTS_ACTION_SERVER_REMOVE);
   d->set_status(Chat::CONTACT_STATUS_REJECTED);
   return sendRequest(request);
}

bool BaseChatClient::sendUpdateMessageState(const ChatDataPtr& message)
{
   assert(message->has_message());

   Chat::Request request;
   auto d = request.mutable_message_change_status();
   d->set_message_id(message->message().id());
   d->set_state(message->message().state());
   return sendRequest(request);
}

bool BaseChatClient::sendSearchUsersRequest(const std::string &userIdPattern)
{
   Chat::Request request;
   auto d = request.mutable_search_users();
   d->set_sender_id(currentUserId_);
   d->set_search_id_pattern(userIdPattern);
   return sendRequest(request);
}

std::string BaseChatClient::deriveKey(const std::string &email) const
{
   return hasher_->deriveKey(email);
}


std::string BaseChatClient::getUserId() const
{
   return currentUserId_;
}

void BaseChatClient::cleanupConnection()
{
   chatSessionKeyPtr_->clearAll();
   currentUserId_.clear();
   currentJwt_.clear();
   connection_.reset();

   OnLogoutCompleted();
}

bool BaseChatClient::decodeAndUpdateIncomingSessionPublicKey(const std::string& senderId, const BinaryData& encodedPublicKey)
{
   // decrypt by ies received public key
   std::unique_ptr<Encryption::IES_Decryption> dec = Encryption::IES_Decryption::create(logger_);
   dec->setPrivateKey(getOwnAuthPrivateKey());

   dec->setData(encodedPublicKey.toBinStr());

   Botan::SecureVector<uint8_t> decodedData;
   try {
      dec->finish(decodedData);
   }
   catch (const std::exception &e) {
      logger_->error("[BaseChatClient::{}] Failed to decrypt public key by ies: {}", __func__, e.what());
      return false;
   }

   BinaryData remoteSessionPublicKey = BinaryData::CreateFromHex(QString::fromUtf8((char*)decodedData.data(), (int)decodedData.size()).toStdString());

   chatSessionKeyPtr_->generateLocalKeysForUser(senderId);
   chatSessionKeyPtr_->updateRemotePublicKeyForUser(senderId, remoteSessionPublicKey);

   return true;
}

void BaseChatClient::OnSendMessageResponse(const Chat::Response_SendMessage& response)
{
   if (response.accepted()) {
      if (!chatDb_->syncMessageId(response.client_message_id(), response.server_message_id())) {
         logger_->error("[BaseChatClient::{}] failed to update message id in DB from {} to {}", 
            __func__, response.client_message_id(), response.server_message_id());
      }

      onMessageSent(response.receiver_id(), response.client_message_id(), response.server_message_id());
   }
}

void BaseChatClient::OnMessageChangeStatusResponse(const Chat::Response_MessageChangeStatus& response)
{
   const std::string &messageId = response.message_id();
   int newStatus = int(response.status());

   if (chatDb_->updateMessageStatus(messageId, newStatus)) {
      std::string chatId = response.sender_id() == currentUserId_
                           ? response.receiver_id()
                           : response.sender_id();

      onMessageStatusChanged(chatId, messageId, newStatus);
   } else {
      logger_->error("[BaseChatClient::{}] failed to update message state in DB: {} {}", 
         __func__, response.message_id(), newStatus);
   }
}

void BaseChatClient::OnModifyContactsDirectResponse(const Chat::Response_ModifyContactsDirect& response)
{
   std::string actionString = "<unknown>";
   const std::string& senderId = response.sender_id();
   const auto publicKey = BinaryData(response.sender_public_key());
   const auto publicKeyTimestamp = QDateTime::fromMSecsSinceEpoch(response.sender_public_key_timestamp());

   switch (response.action()) {
      case Chat::CONTACTS_ACTION_ACCEPT: {
         actionString = "ContactsAction::Accept";
         onFriendRequestAccepted(senderId, publicKey, publicKeyTimestamp);
         break;
      }
      case Chat::CONTACTS_ACTION_REJECT: {
         actionString = "ContactsAction::Reject";
         onFriendRequestRejected(senderId);
         break;
      }
      case Chat::CONTACTS_ACTION_REQUEST: {
         actionString = "ContactsAction::Request";
         const std::string &receiverId = response.receiver_id();
         const auto pubKey = BinaryData(response.sender_public_key());
         if (response.has_message()) {
            auto message = std::make_shared<Chat::Data>(response.message());
            onFriendRequestReceived(receiverId,
                                 senderId,
                                 pubKey,
                                 publicKeyTimestamp,
                                 message);
         } else {
            onFriendRequestReceived(receiverId,
                                 senderId,
                                 pubKey,
                                 publicKeyTimestamp,
                                 nullptr);
         }
         break;
      }
      case Chat::CONTACTS_ACTION_REMOVE: {
         onFriendRequestedRemove(senderId);
         break;
      }
      default:
         break;
   }

   logger_->debug("[BaseChatClient::{}]: Incoming contact action from {}: {}", 
      __func__, senderId, Chat::ContactsAction_Name(response.action()));
}

void BaseChatClient::OnModifyContactsServerResponse(const Chat::Response_ModifyContactsServer & response)
{
   switch (response.requested_action()) {
      case Chat::CONTACTS_ACTION_SERVER_ADD:
         //addOrUpdateContact(QString::fromStdString(response.userId()));
         retrySendQueuedMessages(response.contact_id());
         break;
      case Chat::CONTACTS_ACTION_SERVER_REMOVE:
         if (response.success()) {
            onServerApprovedFriendRemoving(response.contact_id());
         }
         break;
      case Chat::CONTACTS_ACTION_SERVER_UPDATE:
         //addOrUpdateContact(QString::fromStdString(response.userId()), QStringLiteral(""), true);
         break;
      default:
         break;
   }
}

void BaseChatClient::OnContactsListResponse(const Chat::Response_ContactsList & response)
{
   ChatDataVectorPtr checkedList;
   ChatDataVectorPtr absolutelyNewList;
   ChatDataVectorPtr toConfirmKeysList;
   for (const auto& contact : response.contacts()) {
      if (!contact.has_contact_record()) {
         logger_->error("[BaseChatClient::{}] invalid response detected", __func__);
         continue;
      }

      const auto userId = contact.contact_record().contact_id();
      const auto publicKey = BinaryData(contact.contact_record().public_key());
      const auto publicKeyTimestamp = QDateTime::fromMSecsSinceEpoch(contact.contact_record().public_key_timestamp());

      if (!chatDb_->compareLocalData(userId, publicKey, publicKeyTimestamp)) {
         if (chatDb_->isContactExist(userId)) {
            toConfirmKeysList.push_back(std::make_shared<Chat::Data>(contact));
         } else {
            absolutelyNewList.push_back(std::make_shared<Chat::Data>(contact));
         }
         continue;
      }

      checkedList.push_back(std::make_shared<Chat::Data>(contact));
   }

//   if (!checkedList.empty()) {
//      // false - we don't need update contact in db since it is already fine in compareLocalData
//      OnContactListConfirmed(checkedList, false);
//   }

//   if (!toConfirmKeysList.empty()) {
//      // TODO: if confirmed in GUI execute OnContactListConfirmed and remove this comment
//      emit ConfirmContactsNewData(toConfirmKeysList);
//   }

   if (toConfirmKeysList.empty() && absolutelyNewList.empty()) {
      OnContactListConfirmed(checkedList, {}, {});
   } else {
      emit ConfirmContactsNewData(checkedList,
                                  toConfirmKeysList,
                                  absolutelyNewList);
   }
}

void BaseChatClient::OnContactListConfirmed(
   const ChatDataVectorPtr& checked, const ChatDataVectorPtr& keyUpdate, const ChatDataVectorPtr& absolutelyNew)
{
   enum ContactListKeyAction {
      Leave,
      Update,
      Add
   };

   std::vector<std::pair<ChatDataVectorPtr, ContactListKeyAction>> updateLists;

   updateLists.push_back({checked, Leave});
   updateLists.push_back({keyUpdate, Update});
   updateLists.push_back({absolutelyNew, Add});

   ChatDataVectorPtr resultContactsCollection;

   for (auto updateInfo : updateLists) {
      for (const auto& contact : updateInfo.first) {
         const auto& userId = contact->contact_record().contact_id();
         const auto& publicKey = BinaryData(contact->contact_record().public_key());
         const auto& publicKeyTimestamp = QDateTime::fromMSecsSinceEpoch(contact->contact_record().public_key_timestamp());

         contactPublicKeysPtr_->setPublicKey(userId, publicKey);

         switch (updateInfo.second) {
            case Update:
               if (!chatDb_->updateContactKey(userId, publicKey, publicKeyTimestamp)) {
                  chatDb_->addKey(userId, publicKey, publicKeyTimestamp);
               }
               break;
            case Add:
               if (!chatDb_->addKey(userId, publicKey, publicKeyTimestamp)) {
                  chatDb_->updateContactKey(userId, publicKey, publicKeyTimestamp);
               }
               break;
            default:
               break;
         }
      }
      resultContactsCollection.insert(resultContactsCollection.end(), updateInfo.first.begin(), updateInfo.first.end());
   }

   onContactListLoaded(resultContactsCollection);
}

void BaseChatClient::OnChatroomsList(const Chat::Response_ChatroomsList &response)
{
   ChatDataVectorPtr newList;
   for (const auto& room : response.rooms()) {
      if (!room.has_room()) {
         logger_->error("[BaseChatClient::{}] invalid response detected", __func__);
         continue;
      }

      chatDb_->removeRoomMessages(room.room().id());
      newList.push_back(std::make_shared<Chat::Data>(room));
   }

   onRoomsLoaded(newList);
}

void BaseChatClient::OnRoomMessages(const Chat::Response_RoomMessages& response)
{
   for (const auto &msg : response.messages()) {
      if (!msg.has_message()) {
         logger_->error("[BaseChatClient::{}] invalid response detected", __func__);
         continue;
      }

      auto msgCopy = std::make_shared<Chat::Data>(msg);
      ChatUtils::messageFlagSet(msgCopy->mutable_message(), Chat::Data_Message_State_ACKNOWLEDGED);

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


      onRoomMessageReceived(msgCopy);
   }
}

void BaseChatClient::OnSearchUsersResponse(const Chat::Response_SearchUsers & response)
{
   ChatDataVectorPtr newList;
   for (const auto& user : response.users()) {
      if (!user.has_user()) {
         logger_->error("[BaseChatClient::{}] invalid response detected", __func__);
         continue;
      }
      newList.push_back(std::make_shared<Chat::Data>(user));
   }

   onSearchResult(newList);
}

void BaseChatClient::OnUsersList(const Chat::Response_UsersList& response)
{
   std::vector<std::string> usersList;
   for (auto& user : response.users()) {
      usersList.push_back(user);

      // if status changed clear session keys for contact
      chatSessionKeyPtr_->clearSessionForUser(user);
   }

   onUserListChanged(response.command(), usersList);
}

void BaseChatClient::OnMessages(const Chat::Response_Messages &response)
{
   ChatDataVectorPtr messages;
   for (const auto &msg : response.messages()) {
      auto msgCopy = std::make_shared<Chat::Data>(msg);

      msgCopy->set_direction(Chat::Data_Direction_RECEIVED);

      if (!chatDb_->isContactExist(msg.message().sender_id())) {
         continue;
      }

      msgCopy->set_direction(Chat::Data_Direction_RECEIVED);
      ChatUtils::messageFlagSet(msgCopy->mutable_message(), Chat::Data_Message_State_ACKNOWLEDGED);

      switch (msgCopy->message().encryption()) {
         case Chat::Data_Message_Encryption_AEAD:
         {
            const std::string &senderId = msgCopy->message().sender_id();
            const auto& chatSessionKeyDataPtr = chatSessionKeyPtr_->findSessionForUser(senderId);

            if (!chatSessionKeyPtr_ || !chatSessionKeyPtr_->isExchangeForUserSucceeded(senderId)) {
               logger_->error("[BaseChatClient::{}] Can't find public key for sender {}", __func__, senderId);
               ChatUtils::messageFlagSet(msgCopy->mutable_message(), Chat::Data_Message_State_INVALID);
            }
            else {
               BinaryData remotePublicKey(chatSessionKeyDataPtr->remotePublicKey());
               SecureBinaryData localPrivateKey(chatSessionKeyDataPtr->localPrivateKey());

               msgCopy = ChatUtils::decryptMessageAead(logger_, msgCopy->message(), remotePublicKey, localPrivateKey);
               if (!msgCopy) {
                  logger_->error("[BaseChatClient::{}] decrypt message failed", __func__);
                  continue;
               }
            }

            onDMMessageReceived(msgCopy);

            encryptByIESAndSaveMessageInDb(msgCopy);
         }
            break;

         case Chat::Data_Message_Encryption_IES:
         {
            logger_->error("[BaseChatClient::{}] This could not happen! Failed to decrypt msg.", __func__);
            chatDb_->add(msgCopy);
            auto decMsg = decryptIESMessage(msgCopy);
            onDMMessageReceived(decMsg);
            break;
         }

         default:
            break;
      }
      sendUpdateMessageState(msgCopy);
   }
}

void BaseChatClient::OnAskForPublicKey(const Chat::Response_AskForPublicKey &response)
{
   logger_->debug("[BaseChatClient::{}] Received request to send own public key from server", __func__);

   // Make sure we are the node for which a public key was expected, if not, ignore this call.
   if (currentUserId_ != response.peer_id()) {
      return;
   }

   // Send our key to the peer.
   Chat::Request request;
   auto d = request.mutable_send_own_public_key();
   d->set_receiving_node_id(response.asking_node_id());
   d->set_sending_node_id(response.peer_id());
   sendRequest(request);
}

void BaseChatClient::OnSendOwnPublicKey(const Chat::Response_SendOwnPublicKey &response)
{
   // Make sure we are the node for which a public key was expected, if not, ignore this call.
   if (currentUserId_ != response.receiving_node_id()) {
      return;
   }

   // Save received public key of peer.
   const auto& peerId = response.sending_node_id();
   const auto& publicKey = BinaryData(response.sending_node_public_key());
   const auto& publicKeyTimestamp = QDateTime::fromMSecsSinceEpoch(response.sending_node_public_key_timestamp());
   contactPublicKeysPtr_->setPublicKey(peerId, publicKey);
   chatDb_->updateContactKey(peerId, publicKey, publicKeyTimestamp);

   retrySendQueuedContactRequests(response.sending_node_id());
   retrySendQueuedMessages(response.sending_node_id());
}

bool BaseChatClient::getContacts(ContactRecordDataList &contactList)
{
   return chatDb_->getContacts(contactList);
}

bool BaseChatClient::addOrUpdateContact(const std::string &userId, Chat::ContactStatus status, const std::string &userName)
{
   Chat::Data contact;
   auto d = contact.mutable_contact_record();
   d->set_user_id(userId);
   d->set_contact_id(userId);
   d->set_status(status);
   d->set_display_name(userName);

   if (chatDb_->isContactExist(userId))
   {
      return chatDb_->updateContact(contact);
   }

   return chatDb_->addContact(contact);
}


bool BaseChatClient::removeContactFromDB(const std::string &userId)
{
   return chatDb_->removeContact(userId);
}

void BaseChatClient::OnSessionPublicKeyResponse(const Chat::Response_SessionPublicKey& response)
{
   // Do not use base64 after protobuf switch and send binary data as-is
   Chat::Request request;
   auto d = request.mutable_reply_session_public_key();
   d->set_sender_id(currentUserId_);
   d->set_receiver_id(response.sender_id());

   if (!decodeAndUpdateIncomingSessionPublicKey(response.sender_id(), BinaryData(response.sender_session_public_key()))) {
      logger_->error("[BaseChatClient::{}] Failed updating remote public key!", __func__);

      d->set_session_key_error(Chat::SESSION_UNABLE_DECODE_KEY);
      sendRequest(request);
      return;
   }

   // encode own session public key by ies and send as reply
   BinaryData remotePublicKey;
   if (!contactPublicKeysPtr_->findPublicKeyForUser(response.sender_id(), remotePublicKey)) {
      logger_->error("[BaseChatClient::{}] Cannot find remote public key!", __func__);

      d->set_session_key_error(Chat::SESSION_USER_KEY_NOT_FOUND);
      sendRequest(request);
      return;
   }

   try {
      BinaryData encryptedLocalPublicKey = chatSessionKeyPtr_->iesEncryptLocalPublicKey(response.sender_id(), remotePublicKey);

      auto d = request.mutable_reply_session_public_key();
      d->set_sender_session_public_key(encryptedLocalPublicKey.toBinStr());
      d->set_session_key_error(Chat::SESSION_NO_ERROR);
      sendRequest(request);
   }
   catch (std::exception& e) {
      logger_->error("[BaseChatClient::{}] Failed to encrypt msg by ies {}", __func__, e.what());

      d->set_session_key_error(Chat::SESSION_ENCRYPTION_FAILED);
      sendRequest(request);
      return;
   }
}

void BaseChatClient::OnReplySessionPublicKeyResponse(const Chat::Response_ReplySessionPublicKey& response)
{
   if (Chat::SESSION_NO_ERROR != response.session_key_error()) {
      setInvalidAllMessagesForUser(response.sender_id());
      return;
   }

   if (!decodeAndUpdateIncomingSessionPublicKey(response.sender_id(), BinaryData(response.sender_session_public_key()))) {
      logger_->error("[BaseChatClient::{}] Failed updating remote public key!", __func__);

      setInvalidAllMessagesForUser(response.sender_id());
      return;
   }

   retrySendQueuedMessages(response.sender_id());
}

void BaseChatClient::setInvalidAllMessagesForUser(const std::string& userId)
{
   MessagesQueue messages = enqueuedMessages_[userId];
   enqueuedMessages_.erase(userId);

   while (!messages.empty()) {
      ChatDataPtr messageData = messages.front();
      updateMessageStateAndSave(messageData, Chat::Data_Message_State_INVALID);
      messages.pop();
   }
}

ChatDataPtr BaseChatClient::sendMessageDataRequest(const ChatDataPtr& messageData, const std::string &receiver, bool isFromQueue)
{
   messageData->set_direction(Chat::Data_Direction_SENT);

   if (!isFromQueue) {
      if (!encryptByIESAndSaveMessageInDb(messageData))
      {
         logger_->error("[BaseChatClient::{}] failed to encrypt. discarding message", __func__);
         ChatUtils::messageFlagSet(messageData->mutable_message(), Chat::Data_Message_State_INVALID);
         return messageData;
      }

      onDMMessageReceived(messageData);
   }

   if (!chatDb_->isContactExist(receiver)) {
      //make friend request before sending direct message.
      //Enqueue the message to be sent, once our friend request accepted.
      enqueuedMessages_[receiver].push(messageData);
      // we should not send friend request from here. this is user action
      // sendFriendRequest(receiver);
      return messageData;
   }

   // is contact rejected?
   Chat::Data_ContactRecord contact;
   chatDb_->getContact(messageData->message().receiver_id(), &contact);

   if (contact.status() == Chat::CONTACT_STATUS_REJECTED) {
      logger_->error("[BaseChatClient::{}] {} Receiver has rejected state. Discarding message.", __func__, receiver);
      ChatUtils::messageFlagSet(messageData->mutable_message(), Chat::Data_Message_State_INVALID);
      return messageData;
   }

   BinaryData receiverPublicKey;
   if (!contactPublicKeysPtr_->findPublicKeyForUser(receiver, receiverPublicKey)) {
      // Ask for public key from peer. Enqueue the message to be sent, once we receive the
      // necessary public key.
      enqueuedMessages_[receiver].push(messageData);

      // Send our key to the peer.
      Chat::Request request;
      auto d = request.mutable_ask_for_public_key();
      d->set_asking_node_id(currentUserId_);
      d->set_peer_id(receiver);
      sendRequest(request);

      return messageData;
   }

   switch (resolveMessageEncryption(messageData)) {
      case Chat::Data_Message_Encryption_AEAD: {
         auto msgEncrypted = encryptMessageToSendAEAD(receiver, receiverPublicKey, messageData);
         if (msgEncrypted) {
            Chat::Request request;
            auto d = request.mutable_send_message();
            *d->mutable_message() = std::move(*msgEncrypted);
            sendRequest(request);
         } else {
            return messageData;
         }
         break;
      }
      case Chat::Data_Message_Encryption_IES: {
         auto msgEncrypted = encryptMessageToSendIES(receiverPublicKey, messageData);
         if (msgEncrypted) {
            Chat::Request request;
            auto d = request.mutable_send_message();
            *d->mutable_message() = std::move(*msgEncrypted);
            sendRequest(request);
         } else {
            return messageData;
         }
         break;
      }
      default: {
         Chat::Request request;
         auto d = request.mutable_send_message();
         *d->mutable_message() = *messageData;
         sendRequest(request);
      }
   }

   return messageData;
}

void BaseChatClient::retrySendQueuedMessages(const std::string userId)
{
   // Run over enqueued messages if any, and try to send them all now.
   MessagesQueue messages;
   std::swap(messages, enqueuedMessages_[userId]);

   while (!messages.empty()) {
      sendMessageDataRequest(messages.front(), userId, true);
      messages.pop();
   }
}

void BaseChatClient::eraseQueuedMessages(const std::string userId)
{
   enqueuedMessages_.erase(userId);
}

void BaseChatClient::retrySendQueuedContactRequests(const std::string& userId)
{
   auto crMessage = pendingContactRequests_.find(userId);
   if (crMessage != pendingContactRequests_.end()) {
      auto message = crMessage->second;
      pendingContactRequests_.erase(crMessage);
      sendFriendRequestToServer(userId, message, true);
   }
}

void BaseChatClient::eraseQueuedContactRequests(const std::string& userId)
{
   auto crMessage = pendingContactRequests_.find(userId);
   if (crMessage != pendingContactRequests_.end()) {
      pendingContactRequests_.erase(crMessage);
   }
}

bool BaseChatClient::encryptByIESAndSaveMessageInDb(const ChatDataPtr& message)
{
   auto msgEncrypted = ChatUtils::encryptMessageIes(logger_, message->message(), getOwnAuthPublicKey());

   if (!msgEncrypted) {
      logger_->error("[BaseChatClient::{}] failed to encrypt msg by ies", __func__);
      return false;
   }

   bool result = chatDb_->add(msgEncrypted);
   if (!result) {
      logger_->error("[BaseChatClient::{}] message store failed", __func__);
      return false;
   }

   return true;
}

ChatDataPtr BaseChatClient::encryptMessageToSendAEAD(const std::string &receiver, BinaryData &rpk, ChatDataPtr messageData)
{
   const auto& chatSessionKeyDataPtr = chatSessionKeyPtr_->findSessionForUser(receiver);
   if (chatSessionKeyDataPtr == nullptr || !chatSessionKeyPtr_->isExchangeForUserSucceeded(receiver)) {
      enqueuedMessages_[receiver].push(messageData);

      chatSessionKeyPtr_->generateLocalKeysForUser(receiver);

      BinaryData remotePublicKey(rpk);
      logger_->debug("[BaseChatClient::{}] USING PUBLIC KEY: {}", __func__, remotePublicKey.toHexStr());

      try {
         BinaryData encryptedLocalPublicKey = chatSessionKeyPtr_->iesEncryptLocalPublicKey(receiver, remotePublicKey);

         Chat::Request request;
         auto d = request.mutable_session_public_key();
         d->set_sender_id(currentUserId_);
         d->set_receiver_id(receiver);
         d->set_sender_session_public_key(encryptedLocalPublicKey.toBinStr());
         sendRequest(request);

         return nullptr;
      } catch (std::exception& e) {
         logger_->error("[BaseChatClient::{}] Failed to encrypt msg by ies {}", __func__, e.what());
         return nullptr;
      }
   }

   // search active message session for given user
   const auto userNoncesIterator = userNonces_.find(receiver);
   Botan::SecureVector<uint8_t> nonce;
   if (userNoncesIterator == userNonces_.end()) {
      // generate random nonce
      Botan::AutoSeeded_RNG rng;
      nonce = rng.random_vec(ChatUtils::defaultNonceSize());
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

   auto msgEncrypted = ChatUtils::encryptMessageAead(
      logger_, messageData->message(), 
      chatSessionKeyDataPtr->remotePublicKey(), 
      chatSessionKeyDataPtr->localPrivateKey(), 
      BinaryData(nonce.data(), nonce.size())
      );

   if (!msgEncrypted) {
      logger_->error("[BaseChatClient::{}] can't encode data", __func__);
      ChatUtils::messageFlagSet(messageData->mutable_message(), Chat::Data_Message_State_INVALID);
      return nullptr;
   }

   return msgEncrypted;
}

ChatDataPtr BaseChatClient::encryptMessageToSendIES(BinaryData &rpk, ChatDataPtr messageData)
{
   auto msgEncrypted = ChatUtils::encryptMessageIes(logger_, messageData->message(), rpk);

   if (!msgEncrypted) {
      logger_->error("[BaseChatClient::{}] failed to encrypt msg by ies", __func__);
      ChatUtils::messageFlagSet(messageData->mutable_message(), Chat::Data_Message_State_INVALID);
      return nullptr;
   }

   return msgEncrypted;
}

ChatDataPtr BaseChatClient::decryptIESMessage(const ChatDataPtr& message)
{
   auto msgDecrypted = ChatUtils::decryptMessageIes(logger_, message->message(), getOwnAuthPrivateKey());
   if (!msgDecrypted) {
      logger_->error("[BaseChatClient::{}] Failed to decrypt msg from DB {}", __func__, message->message().id());
      ChatUtils::messageFlagSet(message->mutable_message(), Chat::Data_Message_State_INVALID);

      return message;
   }

   return msgDecrypted;
}

void BaseChatClient::onFriendRequestReceived(const std::string &userId, const std::string &contactId, BinaryData publicKey, const QDateTime& publicKeyTimestamp, const ChatDataPtr& message)
{
   // incoming public key was replaced by server, it's not directly sent by client
   contactPublicKeysPtr_->setPublicKey(contactId, publicKey);
   chatDb_->addKey(contactId, publicKey, publicKeyTimestamp);

   onFriendRequest(userId, contactId, publicKey);

   if (message) {
      message->set_direction(Chat::Data_Direction_RECEIVED);

      switch (message->message().encryption()) {
         case Chat::Data_Message_Encryption_IES: {
            chatDb_->add(message);
            auto decMsg = decryptIESMessage(message);
            onCRMessageReceived(decMsg);
            break;
         }
         case Chat::Data_Message_Encryption_UNENCRYPTED: {
            encryptByIESAndSaveMessageInDb(message);
            onCRMessageReceived(message);
            break;
         }
         default:
            ChatUtils::messageFlagSet(message->mutable_message(), Chat::Data_Message_State_INVALID);
            onCRMessageReceived(message);
            break;
      }
   }
}

void BaseChatClient::onFriendRequestAccepted(const std::string &contactId, BinaryData publicKey, const QDateTime& publicKeyTimestamp)
{
   // incoming public key was replaced by server, it's not directly sent by client
   contactPublicKeysPtr_->setPublicKey(contactId, publicKey);
   chatDb_->addKey(contactId, publicKey, publicKeyTimestamp);

   onContactAccepted(contactId);

   addOrUpdateContact(contactId, Chat::CONTACT_STATUS_ACCEPTED);

   Chat::Request request;
   auto d = request.mutable_modify_contacts_server();
   d->set_sender_id(currentUserId_);
   d->set_contact_id(contactId);
   d->set_action(Chat::CONTACTS_ACTION_SERVER_ADD);
   d->set_status(Chat::CONTACT_STATUS_ACCEPTED);
   sendRequest(request);

   // reprocess message again
   retrySendQueuedMessages(contactId);
}

void BaseChatClient::onFriendRequestRejected(const std::string &contactId)
{
   addOrUpdateContact(contactId, Chat::CONTACT_STATUS_REJECTED);

   onContactRejected(contactId);

   Chat::Request request;
   auto d = request.mutable_modify_contacts_server();
   d->set_sender_id(currentUserId_);
   d->set_contact_id(contactId);
   d->set_action(Chat::CONTACTS_ACTION_SERVER_ADD);
   d->set_status(Chat::CONTACT_STATUS_REJECTED);
   sendRequest(request);

   eraseQueuedMessages(contactId);

}

void BaseChatClient::onFriendRequestedRemove(const std::string &contactId)
{
   eraseQueuedMessages(contactId);
   sendRemoveFriendToServer(contactId);
}

void BaseChatClient::onServerApprovedFriendRemoving(const std::string &contactId)
{
   // remove contact from db and clear session for him
   chatDb_->removeContact(contactId);
   chatSessionKeyPtr_->clearSessionForUser(contactId);

   onContactRemove(contactId);
}

void BaseChatClient::OnConfirmReplacePublicKey(const Chat::Response_ConfirmReplacePublicKey& response)
{
   emit ConfirmUploadNewPublicKey(response.original_public_key(), response.public_key_to_replace());
}

void BaseChatClient::uploadNewPublicKeyToServer(const bool& confirmed)
{
   Chat::Request request;
   auto uploadNewPublicKey = request.mutable_upload_new_public_key();

   uploadNewPublicKey->set_confirmation(
      confirmed ? Chat::Request_UploadNewPublicKey_Confirmation_CONFIRMED : Chat::Request_UploadNewPublicKey_Confirmation_DECLINED);
   uploadNewPublicKey->set_auth_id(currentUserId_);
   uploadNewPublicKey->set_public_key_to_replace(getOwnAuthPublicKey().toBinStr());
   sendRequest(request);
}

void BaseChatClient::OnContactListRejected(const ChatDataVectorPtr& rejectedList)
{
   for (auto contact : rejectedList) {
      onFriendRequestedRemove(contact->contact_record().contact_id());
   }
}

void BaseChatClient::OnContactNewPublicKeyRejected(const std::string& userId)
{
   onFriendRequestedRemove(userId);
}