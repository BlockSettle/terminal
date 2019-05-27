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
#include "ChatTreeModelWrapper.h"
#include <QRegularExpression>

#include "Encryption/AEAD_Encryption.h"
#include "Encryption/AEAD_Decryption.h"
#include "Encryption/IES_Encryption.h"
#include "Encryption/IES_Decryption.h"

#include <QDateTime>
#include <QDebug>

////////////////////////////////////////////////////////////////////////////////
// OTC simulation time periods. Could be removed with whole simulation
////////////////////////////////////////////////////////////////////////////////

static constexpr int acceptCommonOTCTimeout = 1000;
static constexpr int simulateResponseToMyCommonOTCTimeout = 1000;

std::string ChatClient::GetNextRequestorId()
{
   return baseFakeRequestorId_ + std::to_string(nextRequestorId_++);
}

std::string ChatClient::GetNextResponderId()
{
   return baseFakeResponderId_ + std::to_string(nextResponderId_++);
}

std::string ChatClient::GetNextOTCId()
{
   return std::string("client_otc_id_") + std::to_string(nextOtcId_++);
}

std::string ChatClient::GetNextServerOTCId()
{
   return std::string("server_otc_id_") + std::to_string(nextOtcId_++);
}

std::string ChatClient::GetNextResponseId()
{
   return std::string("client_response_") + std::to_string(nextResponseId_++);
}

std::string ChatClient::GetNextServerResponseId()
{
   return std::string("server_response_") + std::to_string(nextResponseId_++);
}

std::string ChatClient::GetNextNegotiationChannelId()
{
   return std::string("otc_channel_") + std::to_string(negotiationChannelId_++);
}

void ChatClient::ScheduleForExpire(const std::shared_ptr<Chat::OTCRequestData>& liveOTCRequest)
{
   const auto expireInterval = liveOTCRequest->expireTimestamp() - QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();

   QTimer::singleShot(expireInterval, [this, otcId = liveOTCRequest->serverRequestId()]
   {
      HandleCommonOTCRequestExpired(otcId);
   });
}

////////////////////////////////////////////////////////////////////////////////

Q_DECLARE_METATYPE(std::shared_ptr<Chat::MessageData>)
Q_DECLARE_METATYPE(std::vector<std::shared_ptr<Chat::MessageData>>)
Q_DECLARE_METATYPE(std::shared_ptr<Chat::RoomData>)
Q_DECLARE_METATYPE(std::vector<std::shared_ptr<Chat::RoomData>>)
Q_DECLARE_METATYPE(std::shared_ptr<Chat::UserData>)
Q_DECLARE_METATYPE(std::vector<std::shared_ptr<Chat::UserData>>)
Q_DECLARE_METATYPE(std::shared_ptr<Chat::OTCRequestData>)
Q_DECLARE_METATYPE(std::shared_ptr<Chat::OTCResponseData>)
Q_DECLARE_METATYPE(std::shared_ptr<Chat::OTCUpdateData>)

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
   qRegisterMetaType<std::shared_ptr<Chat::OTCRequestData>>();
   qRegisterMetaType<std::shared_ptr<Chat::OTCResponseData>>();
   qRegisterMetaType<std::shared_ptr<Chat::OTCUpdateData>>();

   //This is required (with Qt::QueuedConnection), because of ZmqBIP15XDataConnection crashes when delete it from this (callback) thread
   connect(this, &ChatClient::ForceLogoutSignal, this, &ChatClient::onForceLogoutSignal, Qt::QueuedConnection);

   chatDb_ = make_unique<ChatDB>(logger, appSettings_->get<QString>(ApplicationSettings::chatDbFile));
   if (!chatDb_->loadKeys(pubKeys_)) {
      throw std::runtime_error("failed to load chat public keys");
   }

   hasher_ = std::make_shared<UserHasher>();
   model_ = std::make_shared<ChatClientDataModel>();
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
      auto request1 = std::make_shared<Chat::MessagesRequest>("", currentUserId_, currentUserId_);
      sendRequest(request1);
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
         pubKeys_[senderId] = response.getSenderPublicKey();
         chatDb_->addKey(senderId, response.getSenderPublicKey());
         auto contactNode = model_->findContactNode(senderId.toStdString());
         if (contactNode){
            auto data = contactNode->getContactData();
            data->setStatus(Chat::ContactStatus::Accepted);
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
            data->setStatus(Chat::ContactStatus::Rejected);
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
         autheid::PublicKey pk = response.getSenderPublicKey();
         pubKeys_[contactId] = response.getSenderPublicKey();
         chatDb_->addKey(contactId, response.getSenderPublicKey());

         auto contactNode = model_->findContactNode(response.senderId());
         if (contactNode){
            auto data = contactNode->getContactData();
            data->setStatus(Chat::ContactStatus::Accepted);
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
         autheid::PublicKey pk = response.getSenderPublicKey();
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
         citem->setStatus(remote->getContactStatus());
         model_->notifyContactChanged(citem);
      }
      contactsListStr << QString::fromStdString(remote->toJsonString());
      pubKeys_[remote->getContactId()] = remote->getContactPublicKey();
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
   ContactRecordDataList clist;
   chatDb_->getContacts(clist);
   for (auto c : clist) {
      Chat::ContactStatus status = c.getContactStatus();

      auto pk = autheid::PublicKey();

      auto contact =
            std::make_shared<Chat::ContactRecordData>(
               QString::fromStdString(model_->currentUser()),
               c.getContactForId(), status, pk, c.getDisplayName());

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

//void ChatClient::onMessageRead(const std::shared_ptr<Chat::MessageData>& message)
//{
//   addMessageState(message, Chat::MessageData::State::Read);
//}

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
      if (!chatDb_->isContactExist(msg->senderId())) {
         continue;
      }

      msg->setFlag(Chat::MessageData::State::Acknowledged);

      switch (msg->encryptionType()) {
         case Chat::MessageData::EncryptionType::AEAD: {

            const auto& itPublicKey = pubKeys_.find(msg->senderId());

            if (itPublicKey == pubKeys_.end()) {
               logger_->error("[ChatClient::{}] Can't find public key for sender {}",
                              __func__, msg->senderId().toStdString());
               msg->setFlag(Chat::MessageData::State::Invalid);
            }
            else {
               BinaryData remotePublicKey(itPublicKey->second.data(), itPublicKey->second.size());
               SecureBinaryData localPrivateKey(ownPrivKey_.data(), ownPrivKey_.size());

               std::unique_ptr<Encryption::AEAD_Decryption> dec = Encryption::AEAD_Decryption::create(logger_);
               dec->setPrivateKey(localPrivateKey);
               dec->setPublicKey(remotePublicKey);
               dec->setNonce(msg->nonce());
               dec->setData(QByteArray::fromBase64(msg->messageData().toLatin1()).toStdString());
               dec->setAssociatedData(msg->jsonAssociatedData());

               try {
                  Botan::SecureVector<uint8_t> decodedData;
                  dec->finish(decodedData);

                  msg->setMessageData(QString::fromUtf8((char*)decodedData.data(),
                                                        (int)decodedData.size()));
                  msg->setEncryptionType(Chat::MessageData::EncryptionType::Unencrypted);
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
   // Process on main thread because otherwise ChatDB could crash
   QMetaObject::invokeMethod(this, [this, response] {
      response->handle(*this);
   });
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

   if (!chatDb_->isContactExist(messageData.receiverId()))
   {
      //make friend request before sending direct message.
      //Enqueue the message to be sent, once our friend request accepted.
      enqueued_messages_[receiver].push(message);
      sendFriendRequest(messageData.receiverId());
      return result;
   }
   else
   {
      // is contact rejected?
      Chat::ContactRecordData contact(QString(),
                                      QString(),
                                      Chat::ContactStatus::Accepted,
                                      autheid::PublicKey());
      chatDb_->getContact(messageData.receiverId(), contact);

      if (contact.getContactStatus() == Chat::ContactStatus::Rejected)
      {
         logger_->error("[ChatClient::sendOwnMessage] {}",
                        "Receiver has rejected state. Discarding message.");
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

   if (!encryptByIESAndSaveMessageInDb(std::make_shared<Chat::MessageData>(messageData)))
   {
      return result;
   }

   model_->insertContactsMessage(result);

   // search active message session for given user
   const auto userNoncesIterator = userNonces_.find(receiver);
   Botan::SecureVector<uint8_t> nonce;
   if (userNoncesIterator == userNonces_.end()) {
      // generate random nonce
      Botan::AutoSeeded_RNG rng;
      nonce = rng.random_vec(messageData.defaultNonceSize());
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
   SecureBinaryData localPrivateKey(appSettings_->GetAuthKeys().first.data(),
                                    appSettings_->GetAuthKeys().first.size());
   enc->setPrivateKey(localPrivateKey);

   BinaryData remotePublicKey(itPub->second.data(), itPub->second.size());
   enc->setPublicKey(remotePublicKey);

   enc->setNonce(nonce);
   messageData.setNonce(nonce);

   enc->setData(messageData.messageData().toStdString());
   enc->setAssociatedData(messageData.jsonAssociatedData());

   Botan::SecureVector<uint8_t> encodedData;

   try {
      enc->finish(encodedData);
   }
   catch (std::exception & e) {
      logger_->error("[ChatClient::{}] Can't encode data {}", __func__, e.what());
      return result;
   }

   messageData.setMessageData(
            QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(encodedData.data()),
                                           int(encodedData.size())).toBase64()));
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

   auto request = std::make_shared<Chat::SendRoomMessageRequest>(
                     "",
                     receiver.toStdString(),
                     msg.toJsonString());
   sendRequest(request);
   return result;
}

void ChatClient::retrieveUserMessages(const QString &userId)
{
   auto messages = chatDb_->getUserMessages(QString::fromStdString(currentUserId_), userId);
   if (!messages.empty()) {
      for (auto &msg : messages) {
         if (msg->encryptionType() == Chat::MessageData::EncryptionType::IES) {
            decryptIESMessage(msg);
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
            decryptIESMessage(msg);
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
                                   autheid::PublicKey(),
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
            autheid::PublicKey());
   model_->insertContactObject(record);
   auto request =
         std::make_shared<Chat::ContactActionRequestDirect>(
            "",
            currentUserId_,
            friendUserId.toStdString(),
            Chat::ContactsAction::Request,
            appSettings_->GetAuthKeys().second);
   sendRequest(request);
}

void ChatClient::acceptFriendRequest(const QString &friendUserId)
{
   auto contact = model_->findContactItem(friendUserId.toStdString());
   if (!contact) {
      return;
   }
   contact->setStatus(Chat::ContactStatus::Accepted);
   addOrUpdateContact(contact->getContactId(),
                      contact->getContactStatus(),
                      contact->getContactId());

   model_->notifyContactChanged(contact);
   retrieveUserMessages(contact->getContactId());
   auto requestDirect =
         std::make_shared<Chat::ContactActionRequestDirect>(
            "", currentUserId_, friendUserId.toStdString(),
            Chat::ContactsAction::Accept, appSettings_->GetAuthKeys().second);

   sendRequest(requestDirect);
   autheid::PublicKey publicKey = pubKeys_[friendUserId];
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
   contact->setStatus(Chat::ContactStatus::Rejected);
   model_->notifyContactChanged(contact);
   auto request =
         std::make_shared<Chat::ContactActionRequestDirect>(
            "",
            currentUserId_,
            friendUserId.toStdString(),
            Chat::ContactsAction::Reject,
            appSettings_->GetAuthKeys().second);
   sendRequest(request);
   autheid::PublicKey publicKey = pubKeys_[friendUserId];
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
            autheid::PublicKey());

   model_->insertContactObject(record);
   auto requestD =
         std::make_shared<Chat::ContactActionRequestDirect>(
            "",
            currentUserId_,
            userId.toStdString(),
            Chat::ContactsAction::Request,
            appSettings_->GetAuthKeys().second);

   sendRequest(requestD);

   auto requestS =
         std::make_shared<Chat::ContactActionRequestServer>(
            "",
            currentUserId_,
            userId.toStdString(),
            Chat::ContactsActionServer::AddContactRecord,
            Chat::ContactStatus::Outgoing,
            autheid::PublicKey());

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
            autheid::PublicKey());

   sendRequest(requestS);
}

void ChatClient::onActionAcceptContactRequest(std::shared_ptr<Chat::ContactRecordData> crecord)
{
   qDebug() << __func__ << " " << QString::fromStdString(crecord->toJsonString());

   crecord->setStatus(Chat::ContactStatus::Accepted);

   addOrUpdateContact(crecord->getContactId(),
                      crecord->getContactStatus(), crecord->getDisplayName());
   model_->notifyContactChanged(crecord);
   retrieveUserMessages(crecord->getContactId());

   auto request =
         std::make_shared<Chat::ContactActionRequestDirect>(
            "",
            crecord->getContactForId().toStdString(),
            crecord->getContactId().toStdString(),
            Chat::ContactsAction::Accept,
            appSettings_->GetAuthKeys().second);
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
   crecord->setStatus(Chat::ContactStatus::Rejected);

   addOrUpdateContact(crecord->getContactId(),
                      crecord->getContactStatus(),
                      crecord->getDisplayName());
   model_->notifyContactChanged(crecord);

   auto request =
         std::make_shared<Chat::ContactActionRequestDirect>(
            "",
            crecord->getContactForId().toStdString(),
            crecord->getContactId().toStdString(),
            Chat::ContactsAction::Reject,
            appSettings_->GetAuthKeys().second);
   sendRequest(request);
   auto requestS =
         std::make_shared<Chat::ContactActionRequestServer>(
            "",
            currentUserId_,
            crecord->getContactId().toStdString(),
            Chat::ContactsActionServer::UpdateContactRecord,
            Chat::ContactStatus::Rejected,
            autheid::PublicKey());
   sendRequest(requestS);
}

bool ChatClient::onActionIsFriend(const QString& userId)
{
   return isFriend(userId);
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
   enc->setData(message->messageData().toStdString());

   try {
      Botan::SecureVector<uint8_t> encodedData;
      enc->finish(encodedData);

      Chat::MessageData encMessageData(message->senderId(), message->receiverId(), message->id(), message->dateTime(),
         QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(encodedData.data()), int(encodedData.size())).toBase64()), message->state());
      encMessageData.setEncryptionType(Chat::MessageData::EncryptionType::IES);
      encMessageData.setNonce(message->nonce());
      chatDb_->add(encMessageData);
   }
   catch (std::exception & e) {
      logger_->error("[ChatClient::{}] Failed to encrypt msg by ies {}", __func__, e.what());
      return false;
   }

   return true;
}

bool ChatClient::decryptIESMessage(std::shared_ptr<Chat::MessageData>& message)
{
   std::unique_ptr<Encryption::IES_Decryption> dec = Encryption::IES_Decryption::create(logger_);
   SecureBinaryData localPrivateKey(appSettings_->GetAuthKeys().first.data(), appSettings_->GetAuthKeys().first.size());
   dec->setPrivateKey(localPrivateKey);
   dec->setData(QByteArray::fromBase64(message->messageData().toUtf8()).toStdString());

   try {
      Botan::SecureVector<uint8_t> decodedData;
      dec->finish(decodedData);

      message->setMessageData(QString::fromUtf8((char*)decodedData.data(), (int)decodedData.size()));
      message->setEncryptionType(Chat::MessageData::EncryptionType::Unencrypted);
      return true;
   }
   catch (std::exception & e) {
      logger_->error("Failed to decrypt msg from DB {}", message->id().toStdString());
      message->setFlag(Chat::MessageData::State::Invalid);
      return false;
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

bool ChatClient::SubmitCommonOTCRequest(const bs::network::OTCRequest& request)
{
   if (request.ownRequest && !ownSubmittedOTCId_.isEmpty()) {
      logger_->debug("[ChatClient::SubmitCommonOTCRequest] already have own OTC");
      return false;
   }

   if (!sendCommonOTCRequest(request)) {
      logger_->error("[ChatClient::SubmitCommonOTCRequest] failed to sendcommon OTC request");
      return false;
   }

   return true;
}

bool ChatClient::sendCommonOTCRequest(const bs::network::OTCRequest& request)
{
   // XXX - do actual send and return true if message was sent to chat server

   // all down is part of a flow simulation

   const bool simulateResponse = request.ownRequest && request.fakeReplyRequired;

   const QString clientRequestId = QString::fromStdString(GetNextOTCId());
   const QString serverRequestId = QString::fromStdString(GetNextServerOTCId());
   const QString requestorId = QString::fromStdString(request.ownRequest
                                                      ? currentUserId_
                                                      : GetNextRequestorId());
   const QString targetId = Chat::OTCRoomKey;
   const uint64_t submitTimestamp = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
   const uint64_t expireTimestamp = QDateTime::currentDateTimeUtc().addSecs(10*60).toMSecsSinceEpoch();

   auto liveRequest = std::make_shared<Chat::OTCRequestData>(clientRequestId
      , serverRequestId, requestorId, targetId, submitTimestamp, expireTimestamp, request);

   if (request.ownRequest) {
      ownSubmittedOTCId_ = liveRequest->clientRequestId();

      // simulate 1 second delay on accept response
      QTimer::singleShot(acceptCommonOTCTimeout, [this, liveRequest, simulateResponse]
         {
            HandleCommonOTCRequestAccepted(liveRequest);

            if (simulateResponse) {
               QTimer::singleShot(simulateResponseToMyCommonOTCTimeout, [this, liveRequest]
                  {
                     const auto clientResponseId = QString::fromStdString(GetNextResponseId());
                     const auto serverResponseId = QString::fromStdString(GetNextServerResponseId());
                     const auto negotiationChannelId = QString::fromStdString(GetNextNegotiationChannelId());
                     const auto serverRequestId = liveRequest->serverRequestId();
                     const auto requestorId = liveRequest->requestorId();
                     const auto initialTargetId = liveRequest->targetId();
                     const auto responderId = QString::fromStdString(GetNextResponderId());
                     const uint64_t responseTimestamp = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
                     const auto priceRange = bs::network::OTCPriceRange{3300, 3700};;
                     const auto quantityRange = bs::network::OTCRangeID::toQuantityRange(liveRequest->otcRequest().amountRange);

                     auto response = std::make_shared<Chat::OTCResponseData>(clientResponseId
                        , serverResponseId, negotiationChannelId, serverRequestId
                        , requestorId, initialTargetId, responderId
                        , responseTimestamp, priceRange, quantityRange);

                     HandleCommonOTCResponse(response);
                  });
            }
         });
   } else {
      HandleCommonOTCRequest(liveRequest);
   }

   return true;
}

void ChatClient::HandleCommonOTCRequest(const std::shared_ptr<Chat::OTCRequestData>& liveOTCRequest)
{
   aliveOtcRequests_.emplace(liveOTCRequest->serverRequestId().toStdString());
   ScheduleForExpire(liveOTCRequest);

   emit NewOTCRequestReceived(liveOTCRequest);
}

void ChatClient::HandleCommonOTCRequestAccepted(const std::shared_ptr<Chat::OTCRequestData>& liveOTCRequest)
{
   if (liveOTCRequest->clientRequestId() != ownSubmittedOTCId_) {
      logger_->error("[ChatClient::HandleCommonOTCRequestAccepted] looks like not ours OTC was accepted");
      return;
   }

   aliveOtcRequests_.emplace(liveOTCRequest->serverRequestId().toStdString());
   ScheduleForExpire(liveOTCRequest);

   emit OTCRequestAccepted(liveOTCRequest);
}

void ChatClient::HandleCommonOTCRequestRejected(const std::string& rejectReason)
{
   emit OTCOwnRequestRejected(QString::fromStdString(rejectReason));
}

void ChatClient::HandleCommonOTCRequestExpired(const QString& serverOTCId)
{
   auto it = aliveOtcRequests_.find(serverOTCId.toStdString());
   if (it == aliveOtcRequests_.end()) {
      // we were not aware that this OTC request was alive
      return;
   }
   aliveOtcRequests_.erase(it);

   if (serverOTCId == ownServerOTCId_) {
      ownSubmittedOTCId_.clear();
      ownServerOTCId_.clear();
      emit OwnOTCRequestExpired(serverOTCId);
   } else {
      emit OTCRequestExpired(serverOTCId);
   }
}

void ChatClient::HandleCommonOTCRequestCancelled(const QString& serverOTCId)
{
   if (serverOTCId == ownServerOTCId_) {
      ownSubmittedOTCId_.clear();
      ownServerOTCId_.clear();
   }

   auto it = aliveOtcRequests_.find(serverOTCId.toStdString());
   if (it != aliveOtcRequests_.end()) {
      aliveOtcRequests_.erase(it);
      emit OTCRequestCancelled(serverOTCId);
   }
}

void ChatClient::HandleAcceptedCommonOTCResponse(const std::shared_ptr<Chat::OTCResponseData>& response)
{
   model_->insertOTCSentResponse(response->serverResponseId().toStdString());
}

void ChatClient::HandleRejectedCommonOTCResponse(const QString& otcId, const std::string& reason)
{
   emit CommonOTCResponseRejected(otcId, QString::fromStdString(reason));
}

// HandleCommonOTCResponse - handle response we receive to our OTC request
//    sent to common OTC chat room
void ChatClient::HandleCommonOTCResponse(const std::shared_ptr<Chat::OTCResponseData>& response)
{
   model_->insertOTCReceivedResponse(response->serverResponseId().toStdString());
}

// cancel current OTC request sent to OTC chat
bool ChatClient::PullCommonOTCRequest(const QString& serverOTCId)
{
   if (ownServerOTCId_ != serverOTCId) {
      logger_->error("[ChatClient::PullCommonOTCRequest] invalid OTC ID");
      // XXX probably something should be emitted, since chat server will(should) reject pull request
      return false;
   }

   HandleCommonOTCRequestCancelled(serverOTCId);
   return true;
}

bool ChatClient::SubmitCommonOTCResponse(const bs::network::OTCResponse& response)
{
   const auto clientResponseId = QString::fromStdString(GetNextResponseId());
   const auto serverResponseId = QString::fromStdString(GetNextServerResponseId());
   const auto negotiationChannelId = QString::fromStdString(GetNextNegotiationChannelId());
   const auto serverRequestId = response.serverRequestId;
   const auto requestorId = response.requestorId;
   const auto initialTargetId = response.initialTargetId;
   const auto responderId = QString::fromStdString(currentUserId_);
   const uint64_t responseTimestamp = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
   const auto priceRange = response.priceRange;
   const auto quantityRange = response.quantityRange;

   auto liveResponse = std::make_shared<Chat::OTCResponseData>(clientResponseId
      , serverResponseId, negotiationChannelId, serverRequestId, requestorId
      , initialTargetId, responderId, responseTimestamp, priceRange, quantityRange);

   HandleAcceptedCommonOTCResponse(liveResponse);
   return true;
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
