#include "ChatClient.h"

#include <disable_warnings.h>
#include <botan/bigint.h>
#include <botan/base64.h>
#include <botan/auto_rng.h>
#include <enable_warnings.h>

#include "UserHasher.h"
#include "ApplicationSettings.h"
#include "autheid_utils.h"
#include "ChatClientDataModel.h"
#include "UserSearchModel.h"
#include "ChatTreeModelWrapper.h"

#include <QDateTime>
#include <QDebug>
#include <QRegularExpression>

Q_DECLARE_METATYPE(std::shared_ptr<Chat::MessageData>)
Q_DECLARE_METATYPE(std::vector<std::shared_ptr<Chat::MessageData>>)
Q_DECLARE_METATYPE(std::shared_ptr<Chat::UserData>)
Q_DECLARE_METATYPE(std::vector<std::shared_ptr<Chat::UserData>>)

namespace {
   const QRegularExpression rx_email(QLatin1String(R"(^[a-z0-9._-]+@([a-z0-9-]+\.)+[a-z]+$)"), QRegularExpression::CaseInsensitiveOption);
}

ChatClient::ChatClient(const std::shared_ptr<ConnectionManager>& connectionManager
                  , const std::shared_ptr<ApplicationSettings> &appSettings
                  , const std::shared_ptr<spdlog::logger>& logger)

   : BaseChatClient{connectionManager, logger, appSettings->get<QString>(ApplicationSettings::chatDbFile)}
   , appSettings_{appSettings}
{
   qRegisterMetaType<std::shared_ptr<Chat::MessageData>>();
   qRegisterMetaType<std::vector<std::shared_ptr<Chat::MessageData>>>();
   qRegisterMetaType<std::shared_ptr<Chat::UserData>>();
   qRegisterMetaType<std::vector<std::shared_ptr<Chat::UserData>>>();

   model_ = std::make_shared<ChatClientDataModel>();
   userSearchModel_ = std::make_shared<UserSearchModel>();
   model_->setModelChangesHandler(this);
   proxyModel_ = std::make_shared<ChatTreeModelWrapper>();
   proxyModel_->setSourceModel(model_.get());
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

void ChatClient::OnLoginCompleted()
{
   model_->initTreeCategoryGroup();
   emit ConnectedToServer();
   model_->setCurrentUser(currentUserId_);
   readDatabase();
   auto messagesRequest = std::make_shared<Chat::MessagesRequest>("", currentUserId_, currentUserId_);
   sendRequest(messagesRequest);
//      auto request2 = std::make_shared<Chat::ContactsListRequest>("", currentUserId_);
//      sendRequest(request2);
}

void ChatClient::OnLofingFailed()
{
   emit LoginFailed();
}

void ChatClient::OnLogoutCompleted()
{
   model_->clearModel();
   emit LoggedOut();
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

void ChatClient::addMessageState(const std::shared_ptr<Chat::MessageData>& message, Chat::MessageData::State state)
{
   message->setFlag(state);
   if (chatDb_->updateMessageStatus(message->id(), message->state()))
   {
      QString chatId = message->senderId() == QString::fromStdString(currentUserId_)
                    ? message->receiverId()
                    : message->senderId();
      sendUpdateMessageState(message);
   } else {
      message->unsetFlag(state);
   }
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

void ChatClient::sendFriendRequest(const QString &friendUserId)
{
   // TODO

   if (model_->findContactItem(friendUserId.toStdString())) {
      return;
   }

   if (sendFriendRequestToServer(friendUserId)) {
      auto record = std::make_shared<Chat::ContactRecordData>(
            QString::fromStdString(model_->currentUser()),
            friendUserId,
            Chat::ContactStatus::Outgoing,
            BinaryData());
      model_->insertContactObject(record);
      addOrUpdateContact(friendUserId, Chat::ContactStatus::Outgoing);
   } else {
      logger_->error("[ChatClient::sendFriendRequest] failed to send friend request for {}"
                     , friendUserId.toStdString());
   }
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

   sendAcceptFriendRequestToServer(friendUserId);
}

void ChatClient::declineFriendRequest(const QString &friendUserId)
{
   auto contact = model_->findContactItem(friendUserId.toStdString());
   if (!contact) {
      return;
   }
   contact->setContactStatus(Chat::ContactStatus::Rejected);
   model_->notifyContactChanged(contact);

   sendDeclientFriendRequestToServer(friendUserId);
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

   model_->insertContactRequestObject(record);
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
   auto onlineStatus = model_->findContactNode(crecord->getContactId().toStdString())->getOnlineStatus();
   model_->removeContactRequestNode(crecord->getContactId().toStdString());
   model_->insertContactObject(crecord, onlineStatus == ChatContactElement::OnlineStatus::Online);
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

BinaryData ChatClient::getOwnAuthPublicKey() const
{
   const auto publicKey = appSettings_->GetAuthKeys().second;

   return BinaryData(publicKey.data(), publicKey.size());
}

SecureBinaryData ChatClient::getOwnAuthPrivateKey() const
{
   return SecureBinaryData{appSettings_->GetAuthKeys().first.data(), appSettings_->GetAuthKeys().first.size()};
}

std::string ChatClient::getChatServerHost() const
{
   return appSettings_->get<std::string>(ApplicationSettings::chatServerHost);
}

std::string ChatClient::getChatServerPort() const
{
   return appSettings_->get<std::string>(ApplicationSettings::chatServerPort);
}

void ChatClient::onRoomsLoaded(const std::vector<std::shared_ptr<Chat::RoomData>>& roomsList)
{
   for (const auto& room : roomsList) {
      model_->insertRoomObject(room);
   }
   emit RoomsInserted();
}

void ChatClient::onUserListChanged(Chat::UsersListResponse::Command command, const std::vector<std::string>& userList)
{
   for (const auto& user : userList) {
      auto contact = model_->findContactNode(user);
      if (contact) {
         ChatContactElement::OnlineStatus status = ChatContactElement::OnlineStatus::Offline;
         switch (command) {
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
   }
}

void ChatClient::onMessageSent(const QString& receiverId, const QString& localId, const QString& serverId)
{
   auto message = model_->findMessageItem(receiverId.toStdString(), localId.toStdString());
   if (message){
      message->setId(serverId);
      message->setFlag(Chat::MessageData::State::Sent);
      model_->notifyMessageChanged(message);
   } else {
      logger_->error("[ChatClient::onMessageSent] message not found: {}"
                     , localId.toStdString());
   }
}

void ChatClient::onMessageStatusChanged(const QString& chatId, const QString& messageId, int newStatus)
{
   auto message = model_->findMessageItem(chatId.toStdString(), messageId.toStdString());
   if (message) {
      message->updateState(newStatus);
      model_->notifyMessageChanged(message);
   }
}

void ChatClient::onContactAccepted(const QString& contactId)
{
   auto contactNode = model_->findContactNode(contactId.toStdString());
   if (contactNode) {
      auto holdData = contactNode->getContactData();
      if (contactNode->getType() == ChatUIDefinitions::ChatTreeNodeType::ContactsRequestElement) {
         holdData->setContactStatus(Chat::ContactStatus::Accepted);
         contactNode->setOnlineStatus(ChatContactElement::OnlineStatus::Online);
         //model_->notifyContactChanged(data);
         model_->removeContactRequestNode(holdData->getContactId().toStdString());
         model_->insertContactObject(holdData
                                     , contactNode->getOnlineStatus() == ChatContactElement::OnlineStatus::Online);
      }
   }
}

void ChatClient::onContactRejected(const QString& contactId)
{
   auto contactNode = model_->findContactNode(contactId.toStdString());
   if (contactNode){
      auto data = contactNode->getContactData();
      data->setContactStatus(Chat::ContactStatus::Rejected);
      contactNode->setOnlineStatus(ChatContactElement::OnlineStatus::Online);
      model_->notifyContactChanged(data);
   }
}

void ChatClient::onFriendRequest(const QString& userId, const QString& contactId, const BinaryData& pk)
{
   auto contactNode = model_->findContactNode(contactId.toStdString());
   if (contactNode) {
      auto holdData = contactNode->getContactData();
      if (contactNode->getType() == ChatUIDefinitions::ChatTreeNodeType::ContactsRequestElement) {
         holdData->setContactStatus(Chat::ContactStatus::Accepted);
         contactNode->setOnlineStatus(ChatContactElement::OnlineStatus::Online);
         //model_->notifyContactChanged(data);
         model_->removeContactRequestNode(holdData->getContactId().toStdString());
         model_->insertContactObject(holdData
                                    , contactNode->getOnlineStatus() == ChatContactElement::OnlineStatus::Online);
      }
   } else {
      auto contact = std::make_shared<Chat::ContactRecordData>(userId , contactId, Chat::ContactStatus::Incoming, pk);

      model_->insertContactRequestObject(contact, true);
      addOrUpdateContact(contactId, Chat::ContactStatus::Incoming);

      auto requestS = std::make_shared<Chat::ContactActionRequestServer>(""
               , currentUserId_
               , contactId.toStdString()
               , Chat::ContactsActionServer::AddContactRecord
               , Chat::ContactStatus::Incoming ,pk);
      sendRequest(requestS);

      emit NewContactRequest(contactId);
   }
}

void ChatClient::onContactRemove(const QString& contactId)
{
   auto cNode = model_->findContactNode(contactId.toStdString());
   if (cNode->getType() == ChatUIDefinitions::ChatTreeNodeType::ContactsElement) {
      model_->removeContactNode(contactId.toStdString());
   } else {
      model_->removeContactRequestNode(contactId.toStdString());
   }
}

void ChatClient::onDMMessageReceived(const std::shared_ptr<Chat::MessageData>& messageData)
{
   model_->insertContactsMessage(messageData);
}

void ChatClient::onRoomMessageReceived(const std::shared_ptr<Chat::MessageData>& messageData)
{
   model_->insertRoomMessage(messageData);
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

void ChatClient::loadRoomMessagesFromDB(const QString& roomId)
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

void ChatClient::onContactListLoaded(const std::vector<std::shared_ptr<Chat::ContactRecordData>>& remoteContacts)
{
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
         //retrieveUserMessages(remote->getContactId());
      } else {
         citem->setContactStatus(remote->getContactStatus());
         model_->notifyContactChanged(citem);
      }

      addOrUpdateContact(remote->getContactId(), remote->getContactStatus(), remote->getDisplayName());
   }
}

void ChatClient::onSearchResult(const std::vector<std::shared_ptr<Chat::UserData>>& userList)
{
   model_->insertSearchUserList(userList);

   emit SearchUserListReceived(userList, emailEntered_);
   emailEntered_ = false;
}
