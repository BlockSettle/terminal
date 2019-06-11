#include "ChatClient.h"
#include "ChatProtocol/ChatProtocol.h"

#include <disable_warnings.h>
#include <spdlog/spdlog.h>
#include <botan/bigint.h>
#include <botan/base64.h>
#include <botan/auto_rng.h>
#include <enable_warnings.h>

#include "ZMQ_BIP15X_DataConnection.h"
#include "ChatDB.h"
#include "ConnectionManager.h"
#include "ApplicationSettings.h"
#include "autheid_utils.h"
#include "UserHasher.h"
#include "ChatClientDataModel.h"
#include "UserSearchModel.h"
#include "ChatTreeModelWrapper.h"
#include <QRegularExpression>

#include "Encryption/AEAD_Encryption.h"
#include "Encryption/AEAD_Decryption.h"
#include "Encryption/IES_Encryption.h"
#include "Encryption/IES_Decryption.h"
#include "Encryption/ChatSessionKeyData.h"

#include <QDateTime>
#include <QDebug>

Q_DECLARE_METATYPE(std::shared_ptr<Chat::MessageData>)
Q_DECLARE_METATYPE(std::vector<std::shared_ptr<Chat::MessageData>>)
Q_DECLARE_METATYPE(std::shared_ptr<Chat::RoomData>)
Q_DECLARE_METATYPE(std::vector<std::shared_ptr<Chat::RoomData>>)
Q_DECLARE_METATYPE(std::shared_ptr<Chat::UserData>)
Q_DECLARE_METATYPE(std::vector<std::shared_ptr<Chat::UserData>>)

namespace {
   const QRegularExpression rx_email(QLatin1String(R"(^[a-z0-9._-]+@([a-z0-9-]+\.)+[a-z]+$)"), QRegularExpression::CaseInsensitiveOption);
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

   chatSessionKeyPtr_ = std::make_shared<Chat::ChatSessionKey>(logger);

   //This is required (with Qt::QueuedConnection), because of ZmqBIP15XDataConnection crashes when delete it from this (callback) thread
   connect(this, &ChatClient::ForceLogoutSignal, this, &ChatClient::onForceLogoutSignal, Qt::QueuedConnection);

   chatDb_ = make_unique<ChatDB>(logger, appSettings_->get<QString>(ApplicationSettings::chatDbFile));
   if (!chatDb_->loadKeys(contactPublicKeys_)) {
      throw std::runtime_error("failed to load chat public keys");
   }

   hasher_ = std::make_shared<UserHasher>();
   model_ = std::make_shared<ChatClientDataModel>();
   userSearchModel_ = std::make_shared<UserSearchModel>();
   model_->setModelChangesHandler(this);
   proxyModel_ = std::make_shared<ChatTreeModelWrapper>();
   proxyModel_->setSourceModel(model_.get());

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

std::shared_ptr<UserSearchModel> ChatClient::getUserSearchModel()
{
   return userSearchModel_;
}

std::shared_ptr<ChatTreeModelWrapper> ChatClient::getProxyModel()
{
   return proxyModel_;
}

std::string ChatClient::loginToServer(const std::string& email, const std::string& jwt
   , const ZmqBIP15XDataConnection::cbNewKey &cb)
{
   if (connection_) {
      logger_->error("[ChatClient::loginToServer] connecting with not purged connection");
      return {};
   }

   currentUserId_ = hasher_->deriveKey(email);
   currentJwt_ = jwt;

   connection_ = connectionManager_->CreateZMQBIP15XDataConnection();
   connection_->setCBs(cb);

   if (!connection_->openConnection(
          appSettings_->get<std::string>(ApplicationSettings::chatServerHost),
          appSettings_->get<std::string>(ApplicationSettings::chatServerPort), this)
       )
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
      model_->initTreeCategoryGroup();
      emit ConnectedToServer();
      model_->setCurrentUser(currentUserId_);
      readDatabase();
      auto messagesRequest = std::make_shared<Chat::MessagesRequest>("", currentUserId_, currentUserId_);
      sendRequest(messagesRequest);
//      auto request2 = std::make_shared<Chat::ContactsListRequest>("", currentUserId_);
//      sendRequest(request2);
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

   chatSessionKeyPtr_->clearAll();
}

void ChatClient::OnSendMessageResponse(const Chat::SendMessageResponse& response)
{
   QJsonDocument json(response.toJson());
   logger_->debug("[ChatClient::OnSendMessageResponse]: received: {}",
                  json.toJson(QJsonDocument::Indented).toStdString());

   if (response.getResult() == Chat::SendMessageResponse::Result::Accepted) {
      QString localId = QString::fromStdString(response.clientMessageId());
      QString serverId = QString::fromStdString(response.serverMessageId());
      QString receiverId = QString::fromStdString(response.receiverId());
      auto message = model_->findMessageItem(receiverId.toStdString(), localId.toStdString());
      bool res = false;
      if (message){
         message->setId(serverId);
         message->setFlag(Chat::MessageData::State::Sent);
         model_->notifyMessageChanged(message);
         res = chatDb_->syncMessageId(localId, serverId);
      }

      logger_->debug("[ChatClient::OnSendMessageResponse]: message id sync: {}", res?"Success":"Failed");
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
         contactPublicKeys_[senderId] = response.getSenderPublicKey();
         chatDb_->addKey(senderId, response.getSenderPublicKey());

         auto contactNode = model_->findContactNode(senderId.toStdString());
         if (contactNode) {
            auto data = contactNode->getContactData();
            data->setContactStatus(Chat::ContactStatus::Accepted);
            contactNode->setOnlineStatus(ChatContactElement::OnlineStatus::Online);
            model_->notifyContactChanged(data);
         }

         addOrUpdateContact(senderId, Chat::ContactStatus::Accepted);
         auto requestS =
               std::make_shared<Chat::ContactActionRequestServer>(
                  "",
                  currentUserId_,
                  senderId.toStdString(),
                  Chat::ContactsActionServer::UpdateContactRecord,
                  Chat::ContactStatus::Accepted, response.getSenderPublicKey());
         sendRequest(requestS);
         // reprocess message again
         retrySendQueuedMessages(response.senderId());
      }
      break;
      case Chat::ContactsAction::Reject: {
         actionString = "ContactsAction::Reject";
         addOrUpdateContact(QString::fromStdString(response.senderId()), Chat::ContactStatus::Rejected);
         auto contactNode = model_->findContactNode(response.senderId());
         if (contactNode){
            auto data = contactNode->getContactData();
            data->setContactStatus(Chat::ContactStatus::Rejected);
            contactNode->setOnlineStatus(ChatContactElement::OnlineStatus::Online);
            model_->notifyContactChanged(data);
         }
         auto requestS =
               std::make_shared<Chat::ContactActionRequestServer>(
                  "",
                  currentUserId_,
                  response.senderId(),
                  Chat::ContactsActionServer::UpdateContactRecord,
                  Chat::ContactStatus::Rejected, response.getSenderPublicKey());
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

         auto contactNode = model_->findContactNode(response.senderId());
         if (contactNode){
            auto data = contactNode->getContactData();
            data->setContactStatus(Chat::ContactStatus::Accepted);
            contactNode->setOnlineStatus(ChatContactElement::OnlineStatus::Online);
            model_->notifyContactChanged(data);
         } else {
            auto contact =
                  std::make_shared<Chat::ContactRecordData>(
                     userId,
                     contactId,
                     Chat::ContactStatus::Incoming,
                     pk);
            model_->insertContactObject(contact, true);
            addOrUpdateContact(contactId, Chat::ContactStatus::Incoming);

            auto requestS =
                  std::make_shared<Chat::ContactActionRequestServer>(
                     "",
                     currentUserId_,
                     contactId.toStdString(),
                     Chat::ContactsActionServer::AddContactRecord,
                     Chat::ContactStatus::Incoming,
                     pk);
            sendRequest(requestS);

            emit NewContactRequest(contactId);
         }

         //addOrUpdateContact(QString::fromStdString(response.senderId()), QStringLiteral(""), true);
      }
      break;
      case Chat::ContactsAction::Remove: {
         BinaryData pk = response.getSenderPublicKey();
         auto requestS =
               std::make_shared<Chat::ContactActionRequestServer>(
                  "",
                  currentUserId_,
                  response.senderId(),
                  Chat::ContactsActionServer::RemoveContactRecord,
                  Chat::ContactStatus::Incoming, pk);

         sendRequest(requestS);
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
         retrySendQueuedMessages(response.contactId());
      break;
      case Chat::ContactsActionServer::RemoveContactRecord:
         actionString = "ContactsActionServer::RemoveContactRecord";
         //removeContact(QString::fromStdString(response.userId()));
         if (response.getActionResult() == Chat::ContactsActionServerResult::Success) {
            model_->removeContactNode(response.contactId());
            chatDb_->removeContact(QString::fromStdString(response.contactId()));
            //TODO: Remove pub key
         }
         retrySendQueuedMessages(response.contactId());
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
      auto rit = std::find_if(remoteContacts.begin(), remoteContacts.end(),
                              [local](std::shared_ptr<Chat::ContactRecordData> remote)
      {
                 return local->getContactId() == remote->getContactId();
      });

      if (rit == remoteContacts.end()) {
         chatDb_->removeContact(local->getContactId());
         model_->removeContactNode(local->getContactId().toStdString());
      }
   }

   for (auto remote : remoteContacts) {
      auto citem = model_->findContactItem(remote->getContactId().toStdString());

      if (!citem) {
         model_->insertContactObject(remote);
         retrieveUserMessages(remote->getContactId());
      } else {
         citem->setContactStatus(remote->getContactStatus());
         model_->notifyContactChanged(citem);
      }
      contactsListStr << QString::fromStdString(remote->toJsonString());
      contactPublicKeys_[remote->getContactId()] = remote->getContactPublicKey();
      addOrUpdateContact(remote->getContactId(),
                         remote->getContactStatus(),
                         remote->getDisplayName());
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
   emit RoomsInserted();
   logger_->debug("[ChatClient::OnChatroomsList]: Received chatroom list from server: {}",
                  rooms.join(QLatin1String(", ")).prepend(QLatin1Char('[')).append(QLatin1Char(']')).toStdString()
                  );
}

void ChatClient::OnRoomMessages(const Chat::RoomMessagesResponse& response)
{
   logger_->debug("Received chatroom messages from server (receiver id is chatroom): {}",
                  response.getData());

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
      model_->insertRoomMessage(msg);
   }
}

void ChatClient::OnSearchUsersResponse(const Chat::SearchUsersResponse & response)
{
   QStringList users;

   std::vector<std::shared_ptr<Chat::UserData>> userList = response.getUsersList();
   model_->insertSearchUserList(userList);

   for (auto user : userList){
      users << QString::fromStdString(user->toJsonString());
   }
   emit SearchUserListReceived(userList, emailEntered_);
   emailEntered_ = false;
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
      auto request = std::make_shared<Chat::LogoutRequest>("", currentUserId_, "", "");
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
   ContactRecordDataList clist;
   chatDb_->getContacts(clist);
   for (auto c : clist) {
      Chat::ContactStatus status = c.getContactStatus();

      auto pk = BinaryData();

      auto contact =
            std::make_shared<Chat::ContactRecordData>(
               QString::fromStdString(model_->currentUser()),
               c.getUserId(), status, pk, c.getDisplayName());

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

void ChatClient::onForceLogoutSignal()
{
   logout(false);
}

void ChatClient::addMessageState(const std::shared_ptr<Chat::MessageData>& message, Chat::MessageData::State state)
{
   message->setFlag(state);
   if (chatDb_->updateMessageStatus(message->id(), message->state()))
   {
      QString chatId = message->senderId() == QString::fromStdString(currentUserId_)
                    ? message->receiverId()
                    : message->senderId();
      sendUpdateMessageState(message);
      emit MessageStatusUpdated(message->id(), chatId, message->state());
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
         // if status changed clear session keys for contact 
         chatSessionKeyPtr_->clearSessionForUser(user);

         contact->setOnlineStatus(status);
         model_->notifyContactChanged(contact->getContactData());
      }

   });

}

void ChatClient::OnMessages(const Chat::MessagesResponse &response)
{
   logger_->debug("[ChatClient::{}] Received messages from server: {}",
                  __func__, response.getData());
   std::vector<std::shared_ptr<Chat::MessageData>> messages;
   for (const auto &msgStr : response.getDataList()) {
      auto msg = Chat::MessageData::fromJSON(msgStr);
      msg->setMessageDirection(Chat::MessageData::MessageDirection::Received);

      if (!chatDb_->isContactExist(msg->senderId())) {
         continue;
      }

      msg->setFlag(Chat::MessageData::State::Acknowledged);

      switch (msg->encryptionType()) {
         case Chat::MessageData::EncryptionType::AEAD: {

            std::string senderId = msg->senderId().toStdString();
            const auto& chatSessionKeyDataPtr = chatSessionKeyPtr_->findSessionForUser(senderId);

            if (!chatSessionKeyPtr_->isExchangeForUserSucceeded(senderId)) {
               logger_->error("[ChatClient::{}] Can't find public key for sender {}",
                  __func__, msg->senderId().toStdString());
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
                  logger_->error("[ChatClient::{}] Failed to decrypt aead msg {}", __func__, e.what());
                  msg->setFlag(Chat::MessageData::State::Invalid);
               }
            }

            model_->insertContactsMessage(msg);

            encryptByIESAndSaveMessageInDb(msg);
         }
         break;

         case Chat::MessageData::EncryptionType::IES: {
            logger_->error("[ChatClient::{}] This could not happen! Failed to decrypt msg.", __func__);
            model_->insertContactsMessage(msg);
         }
         break;

         default:
         break;
      }
      sendUpdateMessageState(msg);
   }
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
      BinaryData(appSettings_->GetAuthKeys().second.data(), appSettings_->GetAuthKeys().second.size()));
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
   contactPublicKeys_[peerId] = response.getSendingNodePublicKey();
   chatDb_->addKey(peerId, response.getSendingNodePublicKey());

   // Run over enqueued messages if any, and try to send them all now.
   messages_queue& messages = enqueued_messages_[QString::fromStdString(response.getSendingNodeId())];
   while (!messages.empty()) {
      sendMessageDataRequest(messages.front(), QString::fromStdString(response.getSendingNodeId()));
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
   auto messageData = std::make_shared<Chat::MessageData>(QString::fromStdString(currentUserId_), receiver
      , QString::fromStdString(CryptoPRNG::generateRandom(8).toHexStr())
      , QDateTime::currentDateTimeUtc()
      , message);

   logger_->debug("[ChatClient::sendOwnMessage] {}", message.toStdString());

   return sendMessageDataRequest(messageData, receiver);
}

std::shared_ptr<Chat::MessageData> ChatClient::SubmitPrivateOTCRequest(const bs::network::OTCRequest& otcRequest
   , const QString &receiver)
{
   auto otcMessageData = std::make_shared<Chat::OTCRequestData>(QString::fromStdString(currentUserId_), receiver
      , QString::fromStdString(CryptoPRNG::generateRandom(8).toHexStr())
      , QDateTime::currentDateTimeUtc()
      , otcRequest);

   logger_->debug("[ChatClient::SubmitPrivateOTCRequest] {}", otcMessageData->displayText().toStdString());

   return sendMessageDataRequest(otcMessageData, receiver);
}

std::shared_ptr<Chat::MessageData> ChatClient::SubmitPrivateOTCResponse(const bs::network::OTCResponse& otcResponse
   , const QString &receiver)
{
   auto otcMessageData = std::make_shared<Chat::OTCResponseData>(QString::fromStdString(currentUserId_), receiver
      , QString::fromStdString(CryptoPRNG::generateRandom(8).toHexStr())
      , QDateTime::currentDateTimeUtc()
      , otcResponse);

   logger_->debug("[ChatClient::SubmitPrivateOTCResponse] {}", otcMessageData->displayText().toStdString());

   return sendMessageDataRequest(otcMessageData, receiver);
}

std::shared_ptr<Chat::MessageData> ChatClient::SubmitPrivateCancel(const QString &receiver)
{
   auto otcMessageData = std::make_shared<Chat::OTCCloseTradingData>(QString::fromStdString(currentUserId_), receiver
      , QString::fromStdString(CryptoPRNG::generateRandom(8).toHexStr())
      , QDateTime::currentDateTimeUtc());

   logger_->debug("[ChatClient::SubmitPrivateCancel] to {}", receiver.toStdString());

   return sendMessageDataRequest(otcMessageData, receiver);
}

std::shared_ptr<Chat::MessageData> ChatClient::SubmitPrivateUpdate(const bs::network::OTCUpdate& update, const QString &receiver)
{
   auto otcMessageData = std::make_shared<Chat::OTCUpdateData>(QString::fromStdString(currentUserId_), receiver
      , QString::fromStdString(CryptoPRNG::generateRandom(8).toHexStr())
      , QDateTime::currentDateTimeUtc()
      , update);

   logger_->debug("[ChatClient::SubmitPrivateUpdate] to {}", receiver.toStdString());

   return sendMessageDataRequest(otcMessageData, receiver);
}

std::shared_ptr<Chat::MessageData> ChatClient::sendMessageDataRequest(const std::shared_ptr<Chat::MessageData>& messageData
                                                                      , const QString &receiver)
{
   messageData->setMessageDirection(Chat::MessageData::MessageDirection::Sent);

   if (!chatDb_->isContactExist(receiver)) {
      //make friend request before sending direct message.
      //Enqueue the message to be sent, once our friend request accepted.
      enqueued_messages_[receiver].push(messageData);
      sendFriendRequest(receiver);
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
         logger_->error("[ChatClient::sendMessageDataRequest] {}",
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
      logger_->debug("[ChatClient::sendMessageDataRequest] USING PUBLIC KEY: {}", remotePublicKey.toHexStr());

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
         logger_->error("[ChatClient::sendMessageDataRequest] Failed to encrypt msg by ies {}", e.what());
         return messageData;
      }
   }

   if (!encryptByIESAndSaveMessageInDb(messageData))
   {
      logger_->error("[ChatClient::sendMessageDataRequest] failed to encrypt. discarding message");
      messageData->setFlag(Chat::MessageData::State::Invalid);
      return messageData;
   }

   model_->insertContactsMessage(messageData);

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
      logger_->error("[ChatClient::sendMessageDataRequest] Can't encode data {}", e.what());
      messageData->setFlag(Chat::MessageData::State::Invalid);
      return messageData;
   }

   auto encryptedMessage = messageData->CreateEncryptedMessage(Chat::MessageData::EncryptionType::AEAD
      , QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(encodedData.data()), int(encodedData.size())).toBase64()));

   auto request = std::make_shared<Chat::SendMessageRequest>("", encryptedMessage->toJsonString());
   sendRequest(request);

   return messageData;
}

std::shared_ptr<Chat::MessageData> ChatClient::sendRoomOwnMessage(const QString& message, const QString& receiver)
{
   auto roomMessage = std::make_shared<Chat::MessageData>(QString::fromStdString(currentUserId_), receiver
      , QString::fromStdString(CryptoPRNG::generateRandom(8).toHexStr())
      , QDateTime::currentDateTimeUtc()
      , message);

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
   chatDb_->add(roomMessage);
   model_->insertRoomMessage(roomMessage);

//   if (!msg.encrypt(itPub->second)) {
//      logger_->error("[ChatClient::sendMessage] failed to encrypt message {}"
//         , msg.getId().toStdString());
//   }

   auto request = std::make_shared<Chat::SendRoomMessageRequest>(
                     "",
                     receiver.toStdString(),
                     roomMessage->toJsonString());
   sendRequest(request);
   return roomMessage;
}

void ChatClient::retrieveUserMessages(const QString &userId)
{
   auto messages = chatDb_->getUserMessages(QString::fromStdString(currentUserId_), userId);
   if (!messages.empty()) {
      for (auto &msg : messages) {
         if (msg->encryptionType() == Chat::MessageData::EncryptionType::IES) {
            msg = decryptIESMessage(msg);
         }
         model_->insertContactsMessage(msg);
      }
   }
}

void ChatClient::retrieveRoomMessages(const QString& roomId)
{
   auto messages = chatDb_->getRoomMessages(roomId);
   if (!messages.empty()) {
      for (auto &msg : messages) {
         if (msg->encryptionType() == Chat::MessageData::EncryptionType::IES) {
            msg = decryptIESMessage(msg);
         }
         model_->insertRoomMessage(msg);
      }
   }
}

bool ChatClient::getContacts(ContactRecordDataList &contactList)
{
   return chatDb_->getContacts(contactList);
}

bool ChatClient::addOrUpdateContact(const QString &userId, Chat::ContactStatus status, const QString &userName)
{
   Chat::ContactRecordData contact(userId,
                                   userId,
                                   status,
                                   BinaryData(),
                                   userName);

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

   if (model_->findContactItem(friendUserId.toStdString())) {
      return;
   }

   auto record =
         std::make_shared<Chat::ContactRecordData>(
            QString::fromStdString(model_->currentUser()),
            friendUserId,
            Chat::ContactStatus::Outgoing,
            BinaryData());
   model_->insertContactObject(record);
   auto request =
         std::make_shared<Chat::ContactActionRequestDirect>(
            "",
            currentUserId_,
            friendUserId.toStdString(),
            Chat::ContactsAction::Request,
            BinaryData(appSettings_->GetAuthKeys().second.data(), appSettings_->GetAuthKeys().second.size()));
   sendRequest(request);
   addOrUpdateContact(friendUserId, Chat::ContactStatus::Outgoing);
}

void ChatClient::acceptFriendRequest(const QString &friendUserId)
{
   auto contact = model_->findContactItem(friendUserId.toStdString());
   if (!contact) {
      return;
   }
   contact->setContactStatus(Chat::ContactStatus::Accepted);
   addOrUpdateContact(contact->getContactId(),
                      contact->getContactStatus(),
                      contact->getContactId());

   model_->notifyContactChanged(contact);
   retrieveUserMessages(contact->getContactId());
   auto requestDirect =
         std::make_shared<Chat::ContactActionRequestDirect>(
            "", currentUserId_, friendUserId.toStdString(),
            Chat::ContactsAction::Accept, 
            BinaryData(appSettings_->GetAuthKeys().second.data(), appSettings_->GetAuthKeys().second.size()));

   sendRequest(requestDirect);
   BinaryData publicKey = contactPublicKeys_[friendUserId];
   auto requestRemote =
         std::make_shared<Chat::ContactActionRequestServer>(
            "",
            currentUserId_,
            friendUserId.toStdString(),
            Chat::ContactsActionServer::AddContactRecord,
            Chat::ContactStatus::Accepted,
            publicKey);
   sendRequest(requestRemote);
}

void ChatClient::declineFriendRequest(const QString &friendUserId)
{
   auto contact = model_->findContactItem(friendUserId.toStdString());
   if (!contact) {
      return;
   }
   contact->setContactStatus(Chat::ContactStatus::Rejected);
   model_->notifyContactChanged(contact);
   auto request =
         std::make_shared<Chat::ContactActionRequestDirect>(
            "",
            currentUserId_,
            friendUserId.toStdString(),
            Chat::ContactsAction::Reject,
            BinaryData(appSettings_->GetAuthKeys().second.data(), appSettings_->GetAuthKeys().second.size()));
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
}

void ChatClient::sendUpdateMessageState(const std::shared_ptr<Chat::MessageData>& message)
{
   auto request =
         std::make_shared<Chat::MessageChangeStatusRequest>(
            currentUserId_, message->id().toStdString(), message->state());

   sendRequest(request);
}

void ChatClient::sendSearchUsersRequest(const QString &userIdPattern)
{
   auto request = std::make_shared<Chat::SearchUsersRequest>(
                     "",
                     currentUserId_,
                     userIdPattern.toStdString());

   sendRequest(request);
}

QString ChatClient::deriveKey(const QString &email) const
{
   return QString::fromStdString(hasher_->deriveKey(email.toStdString()));
}

void ChatClient::clearSearch()
{
   model_->clearSearch();
}

bool ChatClient::isFriend(const QString &userId)
{
   return chatDb_->isContactExist(userId);
}

Chat::ContactRecordData ChatClient::getContact(const QString &userId) const
{
   Chat::ContactRecordData contact(QString(),
                                   QString(),
                                   Chat::ContactStatus::Accepted,
                                   BinaryData());
   chatDb_->getContact(userId, contact);
   return contact;
}

QString ChatClient::getUserId()
{
   return QString::fromStdString(currentUserId_);
}

void ChatClient::onActionAddToContacts(const QString& userId)
{

   if (model_->findContactItem(userId.toStdString())) {
      return;
   }

   qDebug() << __func__ << " " << userId;

   auto record =
         std::make_shared<Chat::ContactRecordData>(
            QString::fromStdString(model_->currentUser()),
            userId,
            Chat::ContactStatus::Outgoing,
            BinaryData());

   model_->insertContactObject(record);
   auto requestD =
         std::make_shared<Chat::ContactActionRequestDirect>(
            "",
            currentUserId_,
            userId.toStdString(),
            Chat::ContactsAction::Request,
            BinaryData(appSettings_->GetAuthKeys().second.data(), appSettings_->GetAuthKeys().second.size()));

   sendRequest(requestD);

   auto requestS =
         std::make_shared<Chat::ContactActionRequestServer>(
            "",
            currentUserId_,
            userId.toStdString(),
            Chat::ContactsActionServer::AddContactRecord,
            Chat::ContactStatus::Outgoing,
            BinaryData());

   sendRequest(requestS);
}

void ChatClient::onActionRemoveFromContacts(std::shared_ptr<Chat::ContactRecordData> crecord)
{
   qDebug() << __func__ << " " << QString::fromStdString(crecord->toJsonString());
   auto requestS =
         std::make_shared<Chat::ContactActionRequestServer>(
            "",
            currentUserId_,
            crecord->getContactId().toStdString(),
            Chat::ContactsActionServer::RemoveContactRecord,
            Chat::ContactStatus::Rejected,
            BinaryData());

   sendRequest(requestS);
}

void ChatClient::onActionAcceptContactRequest(std::shared_ptr<Chat::ContactRecordData> crecord)
{
   qDebug() << __func__ << " " << QString::fromStdString(crecord->toJsonString());

   crecord->setContactStatus(Chat::ContactStatus::Accepted);

   addOrUpdateContact(crecord->getContactId(),
                      crecord->getContactStatus(), crecord->getDisplayName());
   model_->notifyContactChanged(crecord);
   retrieveUserMessages(crecord->getContactId());

   auto request =
         std::make_shared<Chat::ContactActionRequestDirect>(
            "",
            crecord->getUserId().toStdString(),
            crecord->getContactId().toStdString(),
            Chat::ContactsAction::Accept,
            BinaryData(appSettings_->GetAuthKeys().second.data(), appSettings_->GetAuthKeys().second.size()));
   sendRequest(request);
   auto requestS =
         std::make_shared<Chat::ContactActionRequestServer>(
            "",
            currentUserId_,
            crecord->getContactId().toStdString(),
            Chat::ContactsActionServer::UpdateContactRecord,
            Chat::ContactStatus::Accepted,
            crecord->getContactPublicKey());
   sendRequest(requestS);

   emit ContactRequestAccepted(crecord->getContactId());
}

void ChatClient::onActionRejectContactRequest(std::shared_ptr<Chat::ContactRecordData> crecord)
{
   qDebug() << __func__ << " " << QString::fromStdString(crecord->toJsonString());
   crecord->setContactStatus(Chat::ContactStatus::Rejected);

   addOrUpdateContact(crecord->getContactId(),
                      crecord->getContactStatus(),
                      crecord->getDisplayName());
   model_->notifyContactChanged(crecord);

   auto request =
         std::make_shared<Chat::ContactActionRequestDirect>(
            "",
            crecord->getUserId().toStdString(),
            crecord->getContactId().toStdString(),
            Chat::ContactsAction::Reject,
            BinaryData(appSettings_->GetAuthKeys().second.data(), appSettings_->GetAuthKeys().second.size()));
   sendRequest(request);
   auto requestS =
         std::make_shared<Chat::ContactActionRequestServer>(
            "",
            currentUserId_,
            crecord->getContactId().toStdString(),
            Chat::ContactsActionServer::UpdateContactRecord,
            Chat::ContactStatus::Rejected,
            BinaryData());
   sendRequest(requestS);
}

bool ChatClient::onActionIsFriend(const QString& userId)
{
   return isFriend(userId);
}

void ChatClient::retrySendQueuedMessages(const std::string userId)
{
   // Run over enqueued messages if any, and try to send them all now.
   messages_queue& messages = enqueued_messages_[QString::fromStdString(userId)];

   while (!messages.empty()) {
      sendMessageDataRequest(messages.front(), QString::fromStdString(userId));
      messages.pop();
   }
}

void ChatClient::eraseQueuedMessages(const std::string userId)
{
   enqueued_messages_.erase(QString::fromStdString(userId));
}

void ChatClient::onActionSearchUsers(const std::string &text)
{
   QString pattern = QString::fromStdString(text);



   QRegularExpressionMatch match = rx_email.match(pattern);
   if (match.hasMatch()) {
      pattern = deriveKey(pattern);
   } else if (static_cast<int>(UserHasher::KeyLength) < pattern.length()
              || pattern.length() < 3) {
      //Initially max key is 12 symbols
      //and search must be triggerred if pattern have length >= 3
      return;
   }
   emailEntered_ = true;
   sendSearchUsersRequest(pattern);
}

void ChatClient::onActionResetSearch()
{
   model_->clearSearch();
}

bool ChatClient::encryptByIESAndSaveMessageInDb(const std::shared_ptr<Chat::MessageData>& message)
{
   BinaryData localPublicKey(appSettings_->GetAuthKeys().second.data(), appSettings_->GetAuthKeys().second.size());
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
      logger_->error("[ChatClient::{}] Failed to encrypt msg by ies {}", __func__, e.what());
      return false;
   }

   return true;
}

std::shared_ptr<Chat::MessageData> ChatClient::decryptIESMessage(const std::shared_ptr<Chat::MessageData>& message)
{
   std::unique_ptr<Encryption::IES_Decryption> dec = Encryption::IES_Decryption::create(logger_);
   SecureBinaryData localPrivateKey(appSettings_->GetAuthKeys().first.data(), appSettings_->GetAuthKeys().first.size());
   dec->setPrivateKey(localPrivateKey);
   dec->setData(QByteArray::fromBase64(message->messagePayload().toUtf8()).toStdString());

   try {
      Botan::SecureVector<uint8_t> decodedData;
      dec->finish(decodedData);

      return message->CreateDecryptedMessage(QString::fromUtf8((char*)decodedData.data(), (int)decodedData.size()));
   }
   catch (std::exception &) {
      logger_->error("Failed to decrypt msg from DB {}", message->id().toStdString());
      message->setFlag(Chat::MessageData::State::Invalid);
      return message;
   }
}

void ChatClient::onMessageRead(std::shared_ptr<Chat::MessageData> message)
{
   if (message->senderId().toStdString() == model_->currentUser()) {
      return;
   }

   message->setFlag(Chat::MessageData::State::Read);
   chatDb_->updateMessageStatus(message->id(), message->state());
   model_->notifyMessageChanged(message);
   sendUpdateMessageState(message);
}


void ChatClient::onRoomMessageRead(std::shared_ptr<Chat::MessageData> message)
{
   message->setFlag(Chat::MessageData::State::Read);
   chatDb_->updateMessageStatus(message->id(), message->state());
   model_->notifyMessageChanged(message);
}

void ChatClient::onContactUpdatedByInput(std::shared_ptr<Chat::ContactRecordData> crecord)
{
   addOrUpdateContact(crecord->getContactId(),
                      crecord->getContactStatus(),
                      crecord->getDisplayName());
}

void ChatClient::OnSessionPublicKeyResponse(const Chat::SessionPublicKeyResponse& response)
{
   if (!decodeAndUpdateIncomingSessionPublicKey(response.senderId(), response.senderSessionPublicKey())) {
      logger_->error("[ChatClient::{}] Failed updating remote public key!", __func__);
      return;
   }

   // encode own session public key by ies and send as reply
   const auto& contactPublicKeyIterator = contactPublicKeys_.find(QString::fromStdString(response.senderId()));
   if (contactPublicKeyIterator == contactPublicKeys_.end()) {
      // this should not happen
      logger_->error("[ChatClient::{}] Cannot find remote public key!", __func__);
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
      logger_->error("[ChatClient::{}] Failed to encrypt msg by ies {}", __func__, e.what());
      return;
   }
}

void ChatClient::OnReplySessionPublicKeyResponse(const Chat::ReplySessionPublicKeyResponse& response)
{
   if (!decodeAndUpdateIncomingSessionPublicKey(response.senderId(), response.senderSessionPublicKey())) {
      logger_->error("[ChatClient::OnReplySessionPublicKeyResponse] Failed updating remote public key!");
      return;
   }

   // Run over enqueued messages if any, and try to send them all now.
   messages_queue messages = enqueued_messages_[QString::fromStdString(response.senderId())];

   while (!messages.empty()) {
      sendMessageDataRequest(messages.front(), QString::fromStdString(response.senderId()));
      messages.pop();
   }
}

bool ChatClient::decodeAndUpdateIncomingSessionPublicKey(const std::string& senderId, const std::string& encodedPublicKey)
{
   BinaryData test(appSettings_->GetAuthKeys().second.data(), appSettings_->GetAuthKeys().second.size());

   // decrypt by ies received public key
   std::unique_ptr<Encryption::IES_Decryption> dec = Encryption::IES_Decryption::create(logger_);
   SecureBinaryData localPrivateKey(appSettings_->GetAuthKeys().first.data(), appSettings_->GetAuthKeys().first.size());
   dec->setPrivateKey(localPrivateKey);

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
