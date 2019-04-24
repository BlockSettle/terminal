#include "ChatClient.h"
#include "ChatProtocol/ChatProtocol.h"

#include <spdlog/spdlog.h>
#include <botan/bigint.h>
#include <botan/base64.h>
#include <botan/auto_rng.h>

#include "ZMQ_BIP15X_DataConnection.h"
#include "ChatDB.h"
#include "ConnectionManager.h"
#include "ApplicationSettings.h"
#include "autheid_utils.h"
#include "UserHasher.h"
#include "ChatClientDataModel.h"

#include <QDateTime>
#include <QDebug>

Q_DECLARE_METATYPE(std::shared_ptr<Chat::MessageData>)
Q_DECLARE_METATYPE(std::vector<std::shared_ptr<Chat::MessageData>>)
Q_DECLARE_METATYPE(std::shared_ptr<Chat::RoomData>)
Q_DECLARE_METATYPE(std::vector<std::shared_ptr<Chat::RoomData>>)
Q_DECLARE_METATYPE(std::shared_ptr<Chat::UserData>)
Q_DECLARE_METATYPE(std::vector<std::shared_ptr<Chat::UserData>>)

//We have current flags
//We have upladed flags
//We need to put flags to updated flags
//But only in places that allowed by mask
static int syncFlagsByMask(int flags, int uflags, int mask){
   int set_mask = mask & uflags;
   int unset_mask = (mask & uflags) ^ mask;
   return (flags & ~unset_mask) | set_mask;
}

ChatClient::ChatClient(const std::shared_ptr<ConnectionManager>& connectionManager
                  , const std::shared_ptr<ApplicationSettings> &appSettings
                  , const std::shared_ptr<spdlog::logger>& logger)

   : connectionManager_(connectionManager)
   , appSettings_(appSettings)
   , logger_(logger)
   , ownPrivKey_(appSettings_->GetAuthKeys().first)
{
   qRegisterMetaType<std::shared_ptr<Chat::MessageData>>();
   qRegisterMetaType<std::vector<std::shared_ptr<Chat::MessageData>>>();
   qRegisterMetaType<std::shared_ptr<Chat::RoomData>>();
   qRegisterMetaType<std::vector<std::shared_ptr<Chat::RoomData>>>();
   qRegisterMetaType<std::shared_ptr<Chat::UserData>>();
   qRegisterMetaType<std::vector<std::shared_ptr<Chat::UserData>>>();

   //This is required (with Qt::QueuedConnection), because of ZmqBIP15XDataConnection crashes when delete it from this (callback) thread
   connect(this, &ChatClient::ForceLogoutSignal, this, &ChatClient::onForceLogoutSignal, Qt::QueuedConnection);

   chatDb_ = make_unique<ChatDB>(logger, appSettings_->get<QString>(ApplicationSettings::chatDbFile));
   if (!chatDb_->loadKeys(pubKeys_)) {
      throw std::runtime_error("failed to load chat public keys");
   }

   hasher_ = std::make_shared<UserHasher>();
   model_ = std::make_shared<ChatClientDataModel>();

   heartbeatTimer_.setInterval(30 * 1000);
   heartbeatTimer_.setSingleShot(false);
   connect(&heartbeatTimer_, &QTimer::timeout, this, &ChatClient::sendHeartbeat);
   //heartbeatTimer_.start();
}

ChatClient::~ChatClient() noexcept
{
   // Let's not call anything here as this could cause crash
}

std::shared_ptr<ChatClientDataModel> ChatClient::getDataModel()
{
   return model_;
}

std::string ChatClient::loginToServer(const std::string& email, const std::string& jwt)
{
   if (connection_) {
      logger_->error("[ChatClient::loginToServer] connecting with not purged connection");
      return std::string();
   }

   currentUserId_ = hasher_->deriveKey(email);
   currentJwt_ = jwt;

   connection_ = connectionManager_->CreateZMQBIP15XDataConnection();

   if (!connection_->openConnection(appSettings_->get<std::string>(ApplicationSettings::chatServerHost)
                            , appSettings_->get<std::string>(ApplicationSettings::chatServerPort), this))
   {
      logger_->error("[ChatClient::loginToServer] failed to open ZMQ data connection");
      connection_.reset();
   }



   return currentUserId_;
}

void ChatClient::OnLoginReturned(const Chat::LoginResponse &response)
{
   if (response.getStatus() == Chat::LoginResponse::Status::LoginOk) {
      loggedIn_ = true;
      model_->setCurrentUser(currentUserId_);
      readDatabase();
      auto request1 = std::make_shared<Chat::MessagesRequest>("", currentUserId_, currentUserId_);
      sendRequest(request1);
      auto request2 = std::make_shared<Chat::ContactsListRequest>("", currentUserId_);
      sendRequest(request2);
   }
   else {
      loggedIn_ = false;
      emit LoginFailed();
   }
}

void ChatClient::OnLogoutResponse(const Chat::LogoutResponse & response)
{
   logger_->debug("[ChatClient::OnLogoutResponse]: Server sent logout response with data: {}", response.getData());
   emit ForceLogoutSignal();
}

void ChatClient::OnSendMessageResponse(const Chat::SendMessageResponse& response)
{
   QJsonDocument json(response.toJson());
   logger_->debug("[ChatClient::OnSendMessageResponse]: received: {}", json.toJson(QJsonDocument::Indented).toStdString());
   if (response.getResult() == Chat::SendMessageResponse::Result::Accepted) {
      QString localId = QString::fromStdString(response.clientMessageId());
      QString serverId = QString::fromStdString(response.serverMessageId());
      QString receiverId = QString::fromStdString(response.receiverId());
      auto message = model_->findMessageItem(receiverId.toStdString(), localId.toStdString());
      if (message){
         message->setId(serverId);
         message->setFlag(Chat::MessageData::State::Sent);
      }
      model_->notifyMessageChanged(message);
      bool res = message && chatDb_->syncMessageId(localId, serverId);

      logger_->debug("[ChatClient::OnSendMessageResponse]: message id sync: {}", res?"Success":"Failed");

      emit MessageIdUpdated(localId, serverId, receiverId);


   }
}

void ChatClient::OnMessageChangeStatusResponse(const Chat::MessageChangeStatusResponse& response)
{
   //TODO: Implement me!
   std::string messageId = response.messageId();
   std::string senderId = response.messageSenderId();
   std::string receiverId = response.messageReceiverId();
   int newStatus = response.getUpdatedStatus();
   logger_->debug("[ChatClient::OnMessageChangeStatusResponse]: Updated message status:"
                  " messageId {}"
                  " senderId {}"
                  " receiverId {}"
                  " status {}",
                  messageId,
                  senderId,
                  receiverId,
                  newStatus);


   if (chatDb_->updateMessageStatus(QString::fromStdString(messageId), newStatus)) {
      QString chatId = QString::fromStdString(response.messageSenderId() == currentUserId_
                    ? response.messageReceiverId()
                    : response.messageSenderId());
      auto message = model_->findMessageItem(chatId.toStdString(), messageId);
      if (message){
         message->updateState(newStatus);
      }
      model_->notifyMessageChanged(message);

      emit MessageStatusUpdated(QString::fromStdString(messageId), chatId, newStatus);
   }
   return;
}

void ChatClient::OnContactsActionResponseDirect(const Chat::ContactsActionResponseDirect& response)
{
   std::string actionString = "<unknown>";
   switch (response.getAction()) {
      case Chat::ContactsAction::Accept: {
         actionString = "ContactsAction::Accept";
         QString senderId = QString::fromStdString(response.senderId());
         pubKeys_[senderId] = response.getSenderPublicKey();
         chatDb_->addKey(senderId, response.getSenderPublicKey());
         auto contactNode = model_->findContactNode(senderId.toStdString());
         if (contactNode){
            auto data = contactNode->getContactData();
            data->setStatus(Chat::ContactStatus::Accepted);
            contactNode->setOnlineStatus(ChatContactElement::OnlineStatus::Online);
            model_->notifyContactChanged(data);
         }
         addOrUpdateContact(senderId, ContactUserData::Status::Friend);
         auto requestS = std::make_shared<Chat::ContactActionRequestServer>("", currentUserId_, senderId.toStdString(), Chat::ContactsActionServer::UpdateContactRecord, Chat::ContactStatus::Accepted, response.getSenderPublicKey());
         sendRequest(requestS);
         emit FriendRequestAccepted({response.senderId()});
         // reprocess message again
         retrySendQueuedMessages(response.senderId());
      }
      break;
      case Chat::ContactsAction::Reject: {
         actionString = "ContactsAction::Reject";
         addOrUpdateContact(QString::fromStdString(response.senderId()), ContactUserData::Status::Rejected);
         auto contactNode = model_->findContactNode(response.senderId());
         if (contactNode){
            auto data = contactNode->getContactData();
            data->setStatus(Chat::ContactStatus::Rejected);
            contactNode->setOnlineStatus(ChatContactElement::OnlineStatus::Online);
            model_->notifyContactChanged(data);
         }
         auto requestS = std::make_shared<Chat::ContactActionRequestServer>("", currentUserId_, response.senderId(), Chat::ContactsActionServer::UpdateContactRecord, Chat::ContactStatus::Rejected, response.getSenderPublicKey());
         sendRequest(requestS);
         //removeContact(QString::fromStdString(response.senderId()));
         emit FriendRequestRejected({response.senderId()});
         eraseQueuedMessages(response.senderId());
      }
      break;
      case Chat::ContactsAction::Request: {
         actionString = "ContactsAction::Request";
         QString senderId = QString::fromStdString(response.senderId());
         QString userId = QString::fromStdString(response.receiverId());
         QString contactId = QString::fromStdString(response.senderId());
         autheid::PublicKey pk = response.getSenderPublicKey();
         pubKeys_[senderId] = response.getSenderPublicKey();
         chatDb_->addKey(senderId, response.getSenderPublicKey());

         auto contactNode = model_->findContactNode(response.senderId());
         if (contactNode){
            auto data = contactNode->getContactData();
            data->setStatus(Chat::ContactStatus::Accepted);
            contactNode->setOnlineStatus(ChatContactElement::OnlineStatus::Online);
            model_->notifyContactChanged(data);
         } else {
            auto contact = std::make_shared<Chat::ContactRecordData>(userId, contactId, Chat::ContactStatus::Incoming, pk);
            model_->insertContactObject(contact, true);
            addOrUpdateContact(senderId, ContactUserData::Status::Incoming);
            auto requestS = std::make_shared<Chat::ContactActionRequestServer>("", currentUserId_, userId.toStdString(), Chat::ContactsActionServer::AddContactRecord, Chat::ContactStatus::Incoming, pk);
            sendRequest(requestS);
         }

         //addOrUpdateContact(QString::fromStdString(response.senderId()), QStringLiteral(""), true);
         emit IncomingFriendRequest({senderId.toStdString()});
      }
      break;
   }
   logger_->debug("[ChatClient::OnContactsActionResponseDirect]: Incoming contact action from {}: {}",
                  response.senderId(),
                  actionString
                  );
}

void ChatClient::OnContactsActionResponseServer(const Chat::ContactsActionResponseServer & response)
{
   std::string actionString = "<unknown>";
   switch (response.getRequestedAction()) {
      case Chat::ContactsActionServer::AddContactRecord:
         actionString = "ContactsActionServer::AddContactRecord";
         //addOrUpdateContact(QString::fromStdString(response.userId()));
         //emit AcceptFriendRequest({response.userId()});
         retrySendQueuedMessages(response.contactId());
      break;
      case Chat::ContactsActionServer::RemoveContactRecord:
         actionString = "ContactsActionServer::RemoveContactRecord";
         //removeContact(QString::fromStdString(response.userId()));
         //emit RejectFriendRequest({response.userId()});
         retrySendQueuedMessages(response.contactId());
      break;
      case Chat::ContactsActionServer::UpdateContactRecord:
         actionString = "ContactsActionServer::UpdateContactRecord";
         //addOrUpdateContact(QString::fromStdString(response.userId()), QStringLiteral(""), true);
         //emit IncomingFriendRequest({response.userId()});
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

   logger_->debug("[ChatClient::OnContactsActionResponseServer]: Reseived response for server contact action:\n"
                  "userID: {}\n"
                  "contactID: {}\n"
                  "requested action: {}\n"
                  "action result:    {}\n"
                  "message:          {}",
                  response.userId(),
                  response.contactId(),
                  actionString,
                  actionResString,
                  response.message()
                  );
}

void ChatClient::OnContactsListResponse(const Chat::ContactsListResponse & response)
{
   QStringList contactsListStr;
   const auto& remoteContacts = response.getContactsList();
   const auto localContacts = model_->getAllContacts();

   for (auto local : localContacts) {
      auto rit = std::find_if(remoteContacts.begin(), remoteContacts.end(), [local](std::shared_ptr<Chat::ContactRecordData> remote){
                 return local->getContactId() == remote->getContactId();
      });

      if (rit == remoteContacts.end()) {
         chatDb_->removeContact(local->getContactId());
         model_->removeContactNode(local->getContactId().toStdString());
      }
   }

   for (auto remote : remoteContacts) {
      auto cnode = model_->findContactNode(remote->getContactId().toStdString());

      if (!cnode) {
         model_->insertContactObject(remote);
         pubKeys_[remote->getContactId()] = remote->getContactPublicKey();

         ContactUserData::Status status = ContactUserData::Status::Rejected;
         switch (remote->getContactStatus()) {
            case Chat::ContactStatus::Accepted:
               status = ContactUserData::Status::Friend;
               break;
            case Chat::ContactStatus::Incoming:
               status = ContactUserData::Status::Incoming;
               break;
            case Chat::ContactStatus::Outgoing:
               status = ContactUserData::Status::Outgoing;
               break;
            case Chat::ContactStatus::Rejected:
               status = ContactUserData::Status::Rejected;
               break;

         }
         addOrUpdateContact(remote->getContactId(), status, remote->getContactId());
         contactsListStr << QString::fromStdString(remote->toJsonString());
      }
   }

   logger_->debug("[ChatClient::OnContactsListResponse]:Received {} contacts, from server: [{}]"
               , QString::number(contactsListStr.size()).toStdString()
               , contactsListStr.join(QLatin1String(", ")).toStdString());
}

void ChatClient::OnChatroomsList(const Chat::ChatroomsListResponse& response)
{
   QStringList rooms;

   std::vector<std::shared_ptr<Chat::RoomData>> roomList = response.getChatRoomList();
   for (auto room : roomList){
      model_->insertRoomObject(room);
      rooms << QString::fromStdString(room->toJsonString());
      chatDb_->removeRoomMessages(room->getId());
   }
   emit RoomsAdd(roomList);
   logger_->debug("[ChatClient::OnChatroomsList]: Received chatroom list from server: {}",
                  rooms.join(QLatin1String(", ")).prepend(QLatin1Char('[')).append(QLatin1Char(']')).toStdString()
                  );
}

void ChatClient::OnRoomMessages(const Chat::RoomMessagesResponse& response)
{
   logger_->debug("Received chatroom messages from server (receiver id is chatroom): {}", response.getData());
   std::vector<std::shared_ptr<Chat::MessageData>> messages;
   for (const auto &msgStr : response.getDataList()) {
      const auto msg = Chat::MessageData::fromJSON(msgStr);
      msg->setFlag(Chat::MessageData::State::Acknowledged);
      chatDb_->add(*msg);

      if (msg->encryptionType() == Chat::MessageData::EncryptionType::IES) {
         if (!msg->decrypt(ownPrivKey_)) {
            logger_->error("Failed to decrypt msg {}", msg->getId().toStdString());
            msg->setFlag(Chat::MessageData::State::Invalid);
         }
         else {
            msg->setEncryptionType(Chat::MessageData::EncryptionType::Unencrypted);
         }
      }
      model_->insertRoomMessage(msg);
      messages.push_back(msg);
      //int mask = old_state ^ msg->getState();
      //sendUpdateMessageState(msg);
   }

   emit RoomMessagesUpdate(messages, false);
}

void ChatClient::OnSearchUsersResponse(const Chat::SearchUsersResponse & response)
{
   QStringList users;

   std::vector<std::shared_ptr<Chat::UserData>> userList = response.getUsersList();
   for (auto user : userList){
      users << QString::fromStdString(user->toJsonString());
   }
   emit SearchUserListReceived(userList);
   logger_->debug("[ChatClient::OnSearchUsersResponse]: Received user list from server: "
                  "{}",
                  users.join(QLatin1String(", ")).prepend(QLatin1Char('[')).append(QLatin1Char(']')).toStdString()
                  );
}

void ChatClient::logout(bool send)
{
   loggedIn_ = false;

   if (!connection_) {
      logger_->error("[ChatClient::logout] Disconnected already");
      return;
   }

   if (send) {
      auto request = std::make_shared<Chat::LogoutRequest>("", currentUserId_, "");
      sendRequest(request);
   }

   currentUserId_.clear();
   currentJwt_.clear();

   connection_.reset();
   model_->clearModel();

   emit LoggedOut();
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

void ChatClient::readDatabase()
{
   ContactUserDataList clist;
   chatDb_->getContacts(clist);
   for (auto c : clist) {
      Chat::ContactStatus status = Chat::ContactStatus::Rejected;
      switch (c.status()) {
         case ContactUserData::Status::Friend:
            status = Chat::ContactStatus::Accepted;
            break;
         case ContactUserData::Status::Incoming:
            status = Chat::ContactStatus::Incoming;
            break;
         case ContactUserData::Status::Outgoing:
            status = Chat::ContactStatus::Outgoing;
            break;
         case ContactUserData::Status::Rejected:
            status = Chat::ContactStatus::Rejected;
            break;

      }

      auto pk = autheid::PublicKey();

      auto contact = std::make_shared<Chat::ContactRecordData>(QString::fromStdString(model_->currentUser()), c.userId(), status, pk);
      model_->insertContactObject(contact);
      retrieveUserMessages(contact->getContactId());
   }
}

void ChatClient::sendHeartbeat()
{
   if (loggedIn_ && connection_->isActive()) {
      sendRequest(std::make_shared<Chat::HeartbeatPingRequest>(currentUserId_));
   }
}

void ChatClient::onMessageRead(const std::shared_ptr<Chat::MessageData>& message)
{
   addMessageState(message, Chat::MessageData::State::Read);
}

void ChatClient::onForceLogoutSignal()
{
   logout(false);
}

void ChatClient::addMessageState(const std::shared_ptr<Chat::MessageData>& message, Chat::MessageData::State state)
{
   message->setFlag(state);
   if (chatDb_->updateMessageStatus(message->getId(), message->getState()))
   {
      QString chatId = message->getSenderId() == QString::fromStdString(currentUserId_)
                    ? message->getReceiverId()
                    : message->getSenderId();
      sendUpdateMessageState(message);
      emit MessageStatusUpdated(message->getId(), chatId, message->getState());
   } else {
      message->unsetFlag(state);
   }
}

void ChatClient::OnHeartbeatPong(const Chat::HeartbeatPongResponse &response)
{
   logger_->debug("[ChatClient::OnHeartbeatPong] {}", response.getData());
}

void ChatClient::OnUsersList(const Chat::UsersListResponse &response)
{
   logger_->debug("Received users list from server: {}", response.getData());
   auto dataList = response.getDataList();

   //This switch for compatibility with old code, if presented somewhere
   switch (response.command()) {
      case Chat::UsersListResponse::Command::Replace:
         emit UsersReplace(dataList);
         break;
      case Chat::UsersListResponse::Command::Add:
         emit UsersAdd(dataList);
         break;
      case Chat::UsersListResponse::Command::Delete:
         emit UsersDel(dataList);
         break;
   }

   std::for_each(dataList.begin(), dataList.end(), [response, this](const std::string& user)
   {
      auto contact = model_->findContactNode(user);
      if (contact) {
         ChatContactElement::OnlineStatus status = ChatContactElement::OnlineStatus::Offline;
         switch (response.command()) {
            case Chat::UsersListResponse::Command::Replace:
               status = ChatContactElement::OnlineStatus::Online;
               break;
            case Chat::UsersListResponse::Command::Add:
               status = ChatContactElement::OnlineStatus::Online;
               break;
            case Chat::UsersListResponse::Command::Delete:
               status = ChatContactElement::OnlineStatus::Offline;
               break;
         }
         contact->setOnlineStatus(status);
         model_->notifyContactChanged(contact->getContactData());
      }

   });

}

void ChatClient::OnMessages(const Chat::MessagesResponse &response)
{
   logger_->debug("[ChatClient::{}] Received messages from server: {}", __func__, response.getData());
   std::vector<std::shared_ptr<Chat::MessageData>> messages;
   for (const auto &msgStr : response.getDataList()) {
      const auto msg = Chat::MessageData::fromJSON(msgStr);
      if (!chatDb_->isContactExist(msg->getSenderId())) {
         continue;
      }
/*
<<<<<<< HEAD
      msg->setFlag(Chat::MessageData::State::Acknowledged);
      chatDb_->add(*msg);


      if (msg->getState() & (int)Chat::MessageData::State::Encrypted) {
         if (!msg->decrypt(ownPrivKey_)) {
            logger_->error("Failed to decrypt msg {}", msg->getId().toStdString());
            msg->setFlag(Chat::MessageData::State::Invalid);
=======*/
      const auto& itPublicKey = pubKeys_.find(msg->getSenderId());
      if (itPublicKey == pubKeys_.end()) {
         logger_->error("[ChatClient::{}] Can't find public key for sender {}", __func__, msg->getSenderId().toStdString());
         msg->setFlag(Chat::MessageData::State::Invalid);
      }
      else {
         if (msg->encryptionType() == Chat::MessageData::EncryptionType::AEAD) {
            BinaryData remotePublicKey(itPublicKey->second.data(), itPublicKey->second.size());
            SecureBinaryData localPrivateKey(ownPrivKey_.data(), ownPrivKey_.size());
            if (!msg->decrypt_aead(remotePublicKey, localPrivateKey, logger_)) {
               logger_->error("[ChatClient::{}] Failed to decrypt msg.", __func__);
               msg->setFlag(Chat::MessageData::State::Invalid);
            }
            else {
               // Encrypt by eis and save in db
               auto localEncMsg = *msg;
               localEncMsg.setFlag(Chat::MessageData::State::Acknowledged);
               localEncMsg.encrypt(appSettings_->GetAuthKeys().second);
               localEncMsg.setEncryptionType(Chat::MessageData::EncryptionType::IES);
               chatDb_->add(localEncMsg);
               msg->setEncryptionType(Chat::MessageData::EncryptionType::Unencrypted);
            }
         }
         else {
            logger_->error("[ChatClient::{}] This could not happend! Failed to decrypt msg.", __func__);
//>>>>>>> dev_server_aead_and_bs_dev
         }
      }

      model_->insertContactsMessage(msg);
      messages.push_back(msg);
      sendUpdateMessageState(msg);
   }

   emit MessagesUpdate(messages, false);
}

void ChatClient::OnAskForPublicKey(const Chat::AskForPublicKeyResponse &response)
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
      appSettings_->GetAuthKeys().second);
   sendRequest(request);
}

void ChatClient::OnSendOwnPublicKey(const Chat::SendOwnPublicKeyResponse &response)
{
   logger_->debug("Received public key of peer from server: {}", response.getData());

   // Make sure we are the node for which a public key was expected, if not, ignore this call.
   if ( currentUserId_ != response.getReceivingNodeId()) {
      return;
   }
   // Save received public key of peer.
   const auto peerId = QString::fromStdString(response.getSendingNodeId());
   pubKeys_[peerId] = response.getSendingNodePublicKey();
   chatDb_->addKey(peerId, response.getSendingNodePublicKey());

   // Run over enqueued messages if any, and try to send them all now.
   std::queue<QString>& messages = enqueued_messages_[QString::fromStdString(
      response.getSendingNodeId())];
   while (!messages.empty()) {
      sendOwnMessage(messages.front(), QString::fromStdString(response.getSendingNodeId()));
      messages.pop();
   }
}

void ChatClient::OnDataReceived(const std::string& data)
{
   auto response = Chat::Response::fromJSON(data);
   if (!response) {
      logger_->error("[ChatClient::OnDataReceived] failed to parse message:\n{}", data);
      return;
   }
   response->handle(*this);
}

void ChatClient::OnConnected()
{
   logger_->debug("[ChatClient::OnConnected]");
   auto loginRequest = std::make_shared<Chat::LoginRequest>("", currentUserId_, currentJwt_);
   sendRequest(loginRequest);
}

void ChatClient::OnDisconnected()
{
   logger_->debug("[ChatClient::OnDisconnected]");
   emit ForceLogoutSignal();
}

void ChatClient::OnError(DataConnectionError errorCode)
{
   logger_->debug("[ChatClient::OnError] {}", errorCode);
}

std::shared_ptr<Chat::MessageData> ChatClient::sendOwnMessage(
      const QString &message, const QString &receiver)
{
   Chat::MessageData messageData(QString::fromStdString(currentUserId_), receiver,
      QString::fromStdString(CryptoPRNG::generateRandom(8).toHexStr()),
      QDateTime::currentDateTimeUtc(), message);
   auto result = std::make_shared<Chat::MessageData>(messageData);

   if (!chatDb_->isContactExist(messageData.getReceiverId()))
   {
      // make friend request before sending direct message. Enqueue the message to be sent, once our friend request accepted.
      enqueued_messages_[receiver].push(message);
      sendFriendRequest(messageData.getReceiverId());
      return result;
   }
   else
   {
      // is contact rejected?
      ContactUserData contact;
      chatDb_->getContact(messageData.getReceiverId(), contact);

      if (contact.status() == ContactUserData::Status::Rejected)
      {
         logger_->error("[ChatClient::sendOwnMessage] {}", "Receiver in rejected state. Discarding message.");
         result->setFlag(Chat::MessageData::State::Invalid);
         return result;
      }
   }

   const auto &itPub = pubKeys_.find(receiver);
   if (itPub == pubKeys_.end()) {
      // Ask for public key from peer. Enqueue the message to be sent, once we receive the
      // necessary public key.
      enqueued_messages_[receiver].push(message);

      // Send our key to the peer.
      auto request = std::make_shared<Chat::AskForPublicKeyRequest>(
         "", // clientId
         currentUserId_,
         receiver.toStdString());
      sendRequest(request);
      return result;
   }

   logger_->debug("[ChatClient::sendMessage] {}", message.toStdString());

   auto localEncMsg = messageData;
   if (!localEncMsg.encrypt(appSettings_->GetAuthKeys().second)) {
      logger_->error("[ChatClient::sendMessage] failed to encrypt by local key");
      return result;
   }
   localEncMsg.setEncryptionType(Chat::MessageData::EncryptionType::IES);

   chatDb_->add(localEncMsg);
   //auto local_msg = std::make_shared<Chat::MessageData>(localEncMsg);
   model_->insertContactsMessage(result);

   // search active message session for given user
   const auto userNoncesIterator = userNonces_.find(receiver);
   autheid::SecureBytes nonce;
   if (userNoncesIterator == userNonces_.end()) {
      // generate random nonce
      Botan::AutoSeeded_RNG rng;
      userNonces_[receiver] = nonce = rng.random_vec(messageData.getDefaultNonceSize());
   }
   else {
      // read nonce and increment
      Botan::BigInt bigIntNonce;
      bigIntNonce.binary_decode(userNoncesIterator->second);
      bigIntNonce++;
      userNonces_[receiver] = nonce = Botan::BigInt::encode_locked(bigIntNonce);
   }

   BinaryData remotePublicKey(itPub->second.data(), itPub->second.size());
   SecureBinaryData localPrivateKey(ownPrivKey_.data(), ownPrivKey_.size());
   if (!messageData.encrypt_aead(remotePublicKey, localPrivateKey, nonce, logger_)) {
      logger_->error("[ChatClient::sendMessage] failed to encrypt by aead {}" , messageData.getId().toStdString());
      return result;
   }

   messageData.setEncryptionType(Chat::MessageData::EncryptionType::AEAD);

   auto request = std::make_shared<Chat::SendMessageRequest>("", messageData.toJsonString());
   sendRequest(request);

   return result;
}

std::shared_ptr<Chat::MessageData> ChatClient::sendRoomOwnMessage(const QString& message, const QString& receiver)
{
   Chat::MessageData msg(QString::fromStdString(currentUserId_), receiver
      , QString::fromStdString(CryptoPRNG::generateRandom(8).toHexStr())
      , QDateTime::currentDateTimeUtc(), message);
   auto result = std::make_shared<Chat::MessageData>(msg);

//   const auto &itPub = pubKeys_.find(receiver);
//   if (itPub == pubKeys_.end()) {
//      // Ask for public key from peer. Enqueue the message to be sent, once we receive the
//      // necessary public key.
//      enqueued_messages_[receiver].push(message);

//      // Send our key to the peer.
//      auto request = std::make_shared<Chat::AskForPublicKeyRequest>(
//         "", // clientId
//         currentUserId_,
//         receiver.toStdString());
//      sendRequest(request);
//      return result;
//   }

   logger_->debug("[ChatClient::sendRoomOwnMessage] {}", message.toStdString());

//   auto localEncMsg = msg;
//   if (!localEncMsg.encrypt(appSettings_->GetAuthKeys().second)) {
//      logger_->error("[ChatClient::sendRoomOwnMessage] failed to encrypt by local key");
//   }
   chatDb_->add(msg);
   model_->insertRoomMessage(result);

//   if (!msg.encrypt(itPub->second)) {
//      logger_->error("[ChatClient::sendMessage] failed to encrypt message {}"
//         , msg.getId().toStdString());
//   }

   auto request = std::make_shared<Chat::SendRoomMessageRequest>("", receiver.toStdString(), msg.toJsonString());
   sendRequest(request);
   return result;
}

void ChatClient::retrieveUserMessages(const QString &userId)
{
   auto messages = chatDb_->getUserMessages(QString::fromStdString(currentUserId_), userId);
   if (!messages.empty()) {
      for (auto &msg : messages) {
         if (msg->encryptionType() == Chat::MessageData::EncryptionType::IES) {
            if (!msg->decrypt(ownPrivKey_)) {
               logger_->error("Failed to decrypt msg from DB {}", msg->getId().toStdString());
               msg->setFlag(Chat::MessageData::State::Invalid);
            }
            else {
               msg->setEncryptionType(Chat::MessageData::EncryptionType::Unencrypted);
            }
         }
         model_->insertContactsMessage(msg);
      }
      emit MessagesUpdate(messages, true);
   }
}

void ChatClient::retrieveRoomMessages(const QString& roomId)
{
   auto messages = chatDb_->getRoomMessages(roomId);
   if (!messages.empty()) {
      for (auto &msg : messages) {
         if (msg->encryptionType() == Chat::MessageData::EncryptionType::IES) {
            if (!msg->decrypt(ownPrivKey_)) {
               logger_->error("Failed to decrypt msg from DB {}", msg->getId().toStdString());
               msg->setFlag(Chat::MessageData::State::Invalid);
            }
            else {
               msg->setEncryptionType(Chat::MessageData::EncryptionType::Unencrypted);
            }
         }
         model_->insertRoomMessage(msg);
      }
      emit RoomMessagesUpdate(messages, true);
   }
}

bool ChatClient::getContacts(ContactUserDataList &contactList)
{
   return chatDb_->getContacts(contactList);
}


bool ChatClient::addOrUpdateContact(const QString &userId, ContactUserData::Status status, const QString &userName)
{
   ContactUserData contact;
   QString newUserName = userName;
   if (newUserName.isEmpty())
   {
      newUserName = userId;
   }
   contact.setUserId(userId);
   contact.setUserName(newUserName);
   contact.setStatus(status);

   if (chatDb_->isContactExist(userId))
   {
      return chatDb_->updateContact(contact);
   }

   return chatDb_->addContact(contact);
}

bool ChatClient::removeContact(const QString &userId)
{
   return chatDb_->removeContact(userId);
}

void ChatClient::sendFriendRequest(const QString &friendUserId)
{
   // TODO
   auto record = std::make_shared<Chat::ContactRecordData>(QString::fromStdString(model_->currentUser()), friendUserId, Chat::ContactStatus::Outgoing, autheid::PublicKey());
   model_->insertContactObject(record);
   auto request = std::make_shared<Chat::ContactActionRequestDirect>("", currentUserId_, friendUserId.toStdString(), Chat::ContactsAction::Request, appSettings_->GetAuthKeys().second);
   sendRequest(request);
}

void ChatClient::acceptFriendRequest(const QString &friendUserId)
{
   auto requestDirect = std::make_shared<Chat::ContactActionRequestDirect>("", currentUserId_, friendUserId.toStdString(), Chat::ContactsAction::Accept, appSettings_->GetAuthKeys().second);
   sendRequest(requestDirect);
   autheid::PublicKey publicKey = pubKeys_[friendUserId];
   auto requestRemote = std::make_shared<Chat::ContactActionRequestServer>("", currentUserId_, friendUserId.toStdString(), Chat::ContactsActionServer::AddContactRecord, Chat::ContactStatus::Accepted, publicKey);
   sendRequest(requestRemote);
}

void ChatClient::declineFriendRequest(const QString &friendUserId)
{
   auto request = std::make_shared<Chat::ContactActionRequestDirect>("", currentUserId_, friendUserId.toStdString(), Chat::ContactsAction::Reject, appSettings_->GetAuthKeys().second);
   sendRequest(request);
   autheid::PublicKey publicKey = pubKeys_[friendUserId];
   auto requestRemote = std::make_shared<Chat::ContactActionRequestServer>("", currentUserId_, friendUserId.toStdString(), Chat::ContactsActionServer::AddContactRecord, Chat::ContactStatus::Rejected, publicKey);
}

void ChatClient::sendUpdateMessageState(const std::shared_ptr<Chat::MessageData>& message)
{
   auto request = std::make_shared<Chat::MessageChangeStatusRequest>(currentUserId_, message->getId().toStdString(), message->getState());
   sendRequest(request);
}

void ChatClient::sendSearchUsersRequest(const QString &userIdPattern)
{
   auto request = std::make_shared<Chat::SearchUsersRequest>("", currentUserId_, userIdPattern.toStdString());
   sendRequest(request);
}

QString ChatClient::deriveKey(const QString &email) const
{
   return QString::fromStdString(hasher_->deriveKey(email.toStdString()));
}

void ChatClient::onActionAddToContacts(const QString& userId)
{
   qDebug() << __func__ << " " << userId;
   auto record = std::make_shared<Chat::ContactRecordData>(QString::fromStdString(model_->currentUser()), userId, Chat::ContactStatus::Outgoing, autheid::PublicKey());
   model_->insertContactObject(record);
   auto requestD = std::make_shared<Chat::ContactActionRequestDirect>("", currentUserId_, userId.toStdString(), Chat::ContactsAction::Request, appSettings_->GetAuthKeys().second);
   sendRequest(requestD);
   auto requestS = std::make_shared<Chat::ContactActionRequestServer>("", currentUserId_, userId.toStdString(), Chat::ContactsActionServer::AddContactRecord, Chat::ContactStatus::Outgoing, autheid::PublicKey());
   sendRequest(requestS);
}

void ChatClient::onActionRemoveFromContacts(std::shared_ptr<Chat::ContactRecordData> crecord)
{
   qDebug() << __func__ << " " << QString::fromStdString(crecord->toJsonString());
}

void ChatClient::onActionAcceptContactRequest(std::shared_ptr<Chat::ContactRecordData> crecord)
{
   qDebug() << __func__ << " " << QString::fromStdString(crecord->toJsonString());
   crecord->setStatus(Chat::ContactStatus::Accepted);
   auto request = std::make_shared<Chat::ContactActionRequestDirect>("", crecord->getContactForId().toStdString()
                                                                     , crecord->getContactId().toStdString()
                                                                     , Chat::ContactsAction::Accept, appSettings_->GetAuthKeys().second);
   sendRequest(request);
   auto requestS = std::make_shared<Chat::ContactActionRequestServer>("", currentUserId_, crecord->getContactId().toStdString(), Chat::ContactsActionServer::UpdateContactRecord, Chat::ContactStatus::Accepted, crecord->getContactPublicKey());
   sendRequest(requestS);
}

void ChatClient::onActionRejectContactRequest(std::shared_ptr<Chat::ContactRecordData> crecord)
{
   qDebug() << __func__ << " " << QString::fromStdString(crecord->toJsonString());
   crecord->setStatus(Chat::ContactStatus::Accepted);
   auto request = std::make_shared<Chat::ContactActionRequestDirect>("", crecord->getContactForId().toStdString()
                                                                     , crecord->getContactId().toStdString()
                                                                     , Chat::ContactsAction::Reject, appSettings_->GetAuthKeys().second);
   sendRequest(request);
   auto requestS = std::make_shared<Chat::ContactActionRequestServer>("", currentUserId_, crecord->getContactId().toStdString(), Chat::ContactsActionServer::UpdateContactRecord, Chat::ContactStatus::Rejected, autheid::PublicKey());
   sendRequest(requestS);
}

void ChatClient::retrySendQueuedMessages(const std::string userId)
{
   // Run over enqueued messages if any, and try to send them all now.
   std::queue<QString>& messages = enqueued_messages_[QString::fromStdString(userId)];

   while (!messages.empty()) {
      sendOwnMessage(messages.front(), QString::fromStdString(userId));
      messages.pop();
   }
}

void ChatClient::eraseQueuedMessages(const std::string userId)
{
   enqueued_messages_.erase(QString::fromStdString(userId));
}
