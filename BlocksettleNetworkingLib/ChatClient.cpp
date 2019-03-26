#include "ChatClient.h"
#include "ChatProtocol/ChatProtocol.h"

#include <spdlog/spdlog.h>
#include "botan/base64.h"

#include "ZmqSecuredDataConnection.h"
#include "ChatDB.h"
#include "ConnectionManager.h"
#include "ApplicationSettings.h"
#include "autheid_utils.h"
#include "UserHasher.h"

#include <QDateTime>

Q_DECLARE_METATYPE(std::shared_ptr<Chat::MessageData>)
Q_DECLARE_METATYPE(std::vector<std::shared_ptr<Chat::MessageData>>)
Q_DECLARE_METATYPE(std::shared_ptr<Chat::ChatRoomData>)
Q_DECLARE_METATYPE(std::vector<std::shared_ptr<Chat::ChatRoomData>>)

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
   qRegisterMetaType<std::shared_ptr<Chat::ChatRoomData>>();
   qRegisterMetaType<std::vector<std::shared_ptr<Chat::ChatRoomData>>>();

   chatDb_ = make_unique<ChatDB>(logger, appSettings_->get<QString>(ApplicationSettings::chatDbFile));
   if (!chatDb_->loadKeys(pubKeys_)) {
      throw std::runtime_error("failed to load chat public keys");
   }

   hasher_ = std::make_shared<UserHasher>();

   heartbeatTimer_.setInterval(30 * 1000);
   heartbeatTimer_.setSingleShot(false);
   connect(&heartbeatTimer_, &QTimer::timeout, this, &ChatClient::sendHeartbeat);
   heartbeatTimer_.start();
}

ChatClient::~ChatClient() noexcept
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

   //auto bytesHash = autheid::getSHA256(email.c_str(), email.size());
   //currentUserId_ = QString::fromStdString(autheid::base64Encode(bytesHash).substr(0, 8)).toLower().toStdString();
   currentUserId_ = hasher_->deriveKey(email);

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

void ChatClient::OnSendMessageResponse(const Chat::SendMessageResponse& response)
{
   QJsonDocument json(response.toJson());
   logger_->debug("[ChatClient::OnSendMessageResponse]: received: {}", json.toJson(QJsonDocument::Indented).toStdString());
   if (response.getResult() == Chat::SendMessageResponse::Result::Accepted) {
      QString localId = QString::fromStdString(response.clientMessageId());
      QString serverId = QString::fromStdString(response.serverMessageId());
      QString receiverId = QString::fromStdString(response.receiverId());
      bool res = chatDb_->syncMessageId(localId, serverId);
      
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
      
      emit MessageStatusUpdated(QString::fromStdString(messageId), chatId, newStatus);
   }
   return;
}

void ChatClient::OnContactsActionResponse(const Chat::ContactsActionResponse& response)
{
   std::string actionString = "<unknown>";
   switch (response.getAction()) {
      case Chat::ContactsAction::Accept:
         actionString = "ContactsAction::Accept";
         addOrUpdateContact(QString::fromStdString(response.senderId()));
      break;
      case Chat::ContactsAction::Reject:
         actionString = "ContactsAction::Reject";
      break;
      case Chat::ContactsAction::Request:
         actionString = "ContactsAction::Request";
         addOrUpdateContact(QString::fromStdString(response.senderId()), QStringLiteral(""), true);
         emit IncomingFriendRequest({response.senderId()});
      break;
   }
   logger_->debug("[ChatClient::OnContactsActionResponse]: Incoming contact action from {}: {}",
                  response.senderId(),
                  actionString
                  );
}

void ChatClient::OnChatroomsList(const Chat::ChatroomsListResponse& response)
{
   QStringList rooms;
   
   std::vector<std::shared_ptr<Chat::ChatRoomData>> roomList = response.getChatRoomList();
   for (auto room : roomList){
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

      if (msg->getState() & (int)Chat::MessageData::State::Encrypted) {
         if (!msg->decrypt(ownPrivKey_)) {
            logger_->error("Failed to decrypt msg {}", msg->getId().toStdString());
            msg->setFlag(Chat::MessageData::State::Invalid);
         }
      }
      messages.push_back(msg);
      //int mask = old_state ^ msg->getState();
      //sendUpdateMessageState(msg);
   }

   emit MessagesUpdate(messages, false);
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

void ChatClient::onMessageRead(const std::shared_ptr<Chat::MessageData>& message)
{
   addMessageState(message, Chat::MessageData::State::Read);
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
      msg->setFlag(Chat::MessageData::State::Acknowledged);
      chatDb_->add(*msg);

      if (msg->getState() & (int)Chat::MessageData::State::Encrypted) {
         if (!msg->decrypt(ownPrivKey_)) {
            logger_->error("Failed to decrypt msg {}", msg->getId().toStdString());
            msg->setFlag(Chat::MessageData::State::Invalid);
         }
      }
      messages.push_back(msg);
      //int mask = old_state ^ msg->getState();
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
}

void ChatClient::OnDisconnected()
{
   logger_->debug("[ChatClient::OnDisconnected]");
}

void ChatClient::OnError(DataConnectionError errorCode)
{
   logger_->debug("[ChatClient::OnError] {}", errorCode);
}

std::shared_ptr<Chat::MessageData> ChatClient::sendOwnMessage(
      const QString &message, const QString &receiver)
{
   Chat::MessageData msg(QString::fromStdString(currentUserId_), receiver
      , QString::fromStdString(CryptoPRNG::generateRandom(8).toHexStr())
      , QDateTime::currentDateTimeUtc(), message);
   auto result = std::make_shared<Chat::MessageData>(msg);

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

   auto localEncMsg = msg;
   if (!localEncMsg.encrypt(appSettings_->GetAuthKeys().second)) {
      logger_->error("[ChatClient::sendMessage] failed to encrypt by local key");
   }
   chatDb_->add(localEncMsg);

   if (!msg.encrypt(itPub->second)) {
      logger_->error("[ChatClient::sendMessage] failed to encrypt message {}"
         , msg.getId().toStdString());
   }

   auto request = std::make_shared<Chat::SendMessageRequest>("", msg.toJsonString());
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
         if (msg->getState() & (int)Chat::MessageData::State::Encrypted) {
            if (!msg->decrypt(ownPrivKey_)) {
               logger_->error("Failed to decrypt msg from DB {}", msg->getId().toStdString());
               msg->setFlag(Chat::MessageData::State::Invalid);
            }
         }
      }
      emit MessagesUpdate(messages, true);
   }
}

void ChatClient::retrieveRoomMessages(const QString& roomId)
{
   auto messages = chatDb_->getRoomMessages(roomId);
   if (!messages.empty()) {
      for (auto &msg : messages) {
         if (msg->getState() & (int)Chat::MessageData::State::Encrypted) {
            if (!msg->decrypt(ownPrivKey_)) {
               logger_->error("Failed to decrypt msg from DB {}", msg->getId().toStdString());
               msg->setFlag(Chat::MessageData::State::Invalid);
            }
         }
      }
      emit MessagesUpdate(messages, true);
   }
}

bool ChatClient::getContacts(ContactUserDataList &contactList)
{
   return chatDb_->getContacts(contactList);
}

bool ChatClient::addOrUpdateContact(const QString &userId, const QString &userName, const bool &isIncomingFriendRequest)
{
   ContactUserData contact;
   QString newUserName = userName;
   if (newUserName.isEmpty())
   {
      newUserName = userId;
   }
   contact.setUserId(userId);
   contact.setUserName(newUserName);
   contact.setIncomingFriendRequest(isIncomingFriendRequest);

   if (chatDb_->isContactExist(userId))
   {
      return chatDb_->updateContact(contact);
   }

   return chatDb_->addContact(contact);
}

void ChatClient::sendFriendRequest(const QString &friendUserId)
{
   // TODO
   auto request = std::make_shared<Chat::ContactActionRequest>("", currentUserId_, friendUserId.toStdString(), Chat::ContactsAction::Request);
   sendRequest(request);
}

void ChatClient::sendUpdateMessageState(const std::shared_ptr<Chat::MessageData>& message)
{
   auto request = std::make_shared<Chat::MessageChangeStatusRequest>(currentUserId_, message->getId().toStdString(), message->getState());
   sendRequest(request);
}
