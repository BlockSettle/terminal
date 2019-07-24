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
#include "ChatProtocol/ChatUtils.h"
#include "ProtobufUtils.h"

#include <QDateTime>
#include <QRegularExpression>


namespace {
   // FIXME: regular expression for email address does not exist. How can we do that better?
   const QRegularExpression rx_email(QLatin1String(R"(^[a-z0-9._-]+@([a-z0-9-]+\.)+[a-z]+$)"), QRegularExpression::CaseInsensitiveOption);
}

ChatClient::ChatClient(const std::shared_ptr<ConnectionManager>& connectionManager
                  , const std::shared_ptr<ApplicationSettings> &appSettings
                  , const std::shared_ptr<spdlog::logger>& logger)

   : BaseChatClient{connectionManager, logger, appSettings->get<QString>(ApplicationSettings::chatDbFile)}
   , appSettings_{appSettings}
{
   ChatUtils::registerTypes();

   model_ = std::make_shared<ChatClientDataModel>(logger_);
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

   Chat::Request request;
   auto d = request.mutable_messages();
   d->set_sender_id(currentUserId_);
   d->set_receiver_id(currentUserId_);
   sendRequest(request);

//      auto request2 = std::make_shared<Chat::ContactsListRequest>("", currentUserId_);
//      sendRequest(request2);
}

void ChatClient::OnLogingFailed()
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

   for (const auto &c : clist) {
      auto contact = std::make_shared<Chat::Data>();
      auto d = contact->mutable_contact_record();
      d->set_user_id(model_->currentUser());
      d->set_contact_id(c.contact_id());
      d->set_status(c.status());
      d->set_display_name(c.display_name());
      d->set_public_key(c.public_key());
      d->set_public_key_timestamp(c.public_key_timestamp());

      model_->insertContactObject(contact);

      retrieveUserMessages(c.contact_id());
   }
}

std::shared_ptr<Chat::Data> ChatClient::sendOwnMessage(
      const std::string &message, const std::string &receiver)
{
   auto messageData = std::make_shared<Chat::Data>();
   initMessage(messageData.get(), receiver);

   auto d = messageData->mutable_message();
   d->set_message(message);

   logger_->debug("[ChatClient::{}] {}", __func__, message);

   return sendMessageDataRequest(messageData, receiver);
}

std::shared_ptr<Chat::Data> ChatClient::SubmitPrivateOTCRequest(const bs::network::OTCRequest& otcRequest
   , const std::string &receiver)
{
   logger_->debug("[ChatClient::{}]", __func__);

   auto otcMessageData = std::make_shared<Chat::Data>();
   initMessage(otcMessageData.get(), receiver);

   auto d = otcMessageData->mutable_message();
   auto otc = d->mutable_otc_request();
   otc->set_side(Chat::OtcSide(otcRequest.side));
   otc->set_range_type(Chat::OtcRangeType(otcRequest.amountRange));

   return sendMessageDataRequest(otcMessageData, receiver);
}

std::shared_ptr<Chat::Data> ChatClient::SubmitPrivateOTCResponse(const bs::network::OTCResponse& otcResponse
   , const std::string &receiver)
{
   auto otcMessageData = std::make_shared<Chat::Data>();
   initMessage(otcMessageData.get(), receiver);

   auto d = otcMessageData->mutable_message();
   auto otc = d->mutable_otc_response();
   otc->set_side(Chat::OtcSide(otcResponse.side));
   otc->mutable_price()->set_lower(otcResponse.priceRange.lower);
   otc->mutable_price()->set_upper(otcResponse.priceRange.upper);
   otc->mutable_quantity()->set_lower(otcResponse.quantityRange.lower);
   otc->mutable_quantity()->set_upper(otcResponse.quantityRange.upper);

   logger_->debug("[ChatClient::{}]", __func__);

   return sendMessageDataRequest(otcMessageData, receiver);
}

std::shared_ptr<Chat::Data> ChatClient::SubmitPrivateCancel(const std::string &receiver)
{
   auto otcMessageData = std::make_shared<Chat::Data>();
   initMessage(otcMessageData.get(), receiver);

   auto d = otcMessageData->mutable_message();
   // Just to select required message type
   d->mutable_otc_close_trading();

   logger_->debug("[ChatClient::{}] to {}", __func__, receiver);

   return sendMessageDataRequest(otcMessageData, receiver);
}

std::shared_ptr<Chat::Data> ChatClient::SubmitPrivateUpdate(const bs::network::OTCUpdate& update, const std::string &receiver)
{
   auto otcMessageData = std::make_shared<Chat::Data>();
   initMessage(otcMessageData.get(), receiver);

   auto d = otcMessageData->mutable_message();
   auto otc = d->mutable_otc_update();
   otc->set_price(update.price);
   otc->set_amount(update.amount);

   logger_->debug("[ChatClient::{}] to {}", __func__, receiver);

   return sendMessageDataRequest(otcMessageData, receiver);
}

std::shared_ptr<Chat::Data> ChatClient::sendRoomOwnMessage(const std::string& message, const std::string& receiver)
{
   auto roomMessage = std::make_shared<Chat::Data>();
   initMessage(roomMessage.get(), receiver);

   auto d = roomMessage->mutable_message();
   d->set_message(message);

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

   logger_->debug("[ChatClient::{}] {}", __func__, message);

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

   Chat::Request request;
   auto r = request.mutable_send_room_message();
   r->set_room_id(receiver);
   *r->mutable_message() = *roomMessage;
   sendRequest(request);

   return roomMessage;
}

void ChatClient::createPendingFriendRequest(const std::string &userId)
{
   addOrUpdateContact(userId, Chat::CONTACT_STATUS_OUTGOING_PENDING);
   auto record = std::make_shared<Chat::Data>();
   auto d = record->mutable_contact_record();
   d->set_user_id(model_->currentUser());
   d->set_contact_id(userId);
   d->set_status(Chat::CONTACT_STATUS_OUTGOING_PENDING);
   model_->insertContactObject(record);
   emit  ContactChanged();
}

void ChatClient::onContactRequestPositiveAction(const std::string &contactId, const std::string& message)
{
   auto citem = model_->findContactItem(contactId);

   if (!citem) {
      return;
   }

   switch (citem->contact_record().status()) {
      case Chat::ContactStatus::CONTACT_STATUS_OUTGOING_PENDING:
         sendFriendRequest(contactId, message);
         break;
      case Chat::ContactStatus::CONTACT_STATUS_INCOMING:
         acceptFriendRequest(contactId);
         break;
      default:
         break;
   }
}

void ChatClient::onContactRequestNegativeAction(const std::string &contactId)
{
   auto citem = model_->findContactItem(contactId);

   if (!citem) {
      return;
   }

   switch (citem->contact_record().status()) {
      case Chat::ContactStatus::CONTACT_STATUS_OUTGOING_PENDING:
         chatDb_->removeContact(contactId);
         model_->removeContactRequestNode(contactId);
         break;
      case Chat::ContactStatus::CONTACT_STATUS_INCOMING:
         rejectFriendRequest(contactId);
         break;
      default:
         break;
   }

}

void ChatClient::sendFriendRequest(const std::string &friendUserId, const std::string& message)
{
   auto citem = model_->findContactItem(friendUserId);

   if (citem && citem->contact_record().status() != Chat::ContactStatus::CONTACT_STATUS_OUTGOING_PENDING) {
      return;
   }

   std::shared_ptr<Chat::Data> messageData = nullptr;
   if (!message.empty()) {
      messageData = std::make_shared<Chat::Data>();
      initMessage(messageData.get(), friendUserId);

      auto d = messageData->mutable_message();
      d->set_message(message);
   }


   if (sendFriendRequestToServer(friendUserId, messageData)) {
      logger_->error("[ChatClient::{}] Friend request sent to {}", __func__, friendUserId);
   } else {
      logger_->error("[ChatClient::{}] failed to send friend request for {}", __func__, friendUserId);
   }
}

void ChatClient::acceptFriendRequest(const std::string &friendUserId)
{
   auto contact = model_->findContactItem(friendUserId);

   if (!contact) {
      return;
   }

   contact->mutable_contact_record()->set_status(Chat::CONTACT_STATUS_ACCEPTED);
   addOrUpdateContact(contact->contact_record().contact_id()
      , contact->contact_record().status(), contact->contact_record().display_name());

   model_->removeContactRequestNode(contact->contact_record().contact_id());
   model_->insertContactObject(contact, true);
   retrieveUserMessages(contact->contact_record().contact_id());
   sendAcceptFriendRequestToServer(contact->contact_record().contact_id());
   emit ContactRequestApproved(contact->contact_record().contact_id());
}

void ChatClient::rejectFriendRequest(const std::string &friendUserId)
{
   auto contact = model_->findContactItem(friendUserId);
   if (!contact) {
      return;
   }

   contact->mutable_contact_record()->set_status(Chat::CONTACT_STATUS_REJECTED);

   addOrUpdateContact(contact->contact_record().contact_id(),
                      contact->contact_record().status(),
                      contact->contact_record().display_name());

   model_->notifyContactChanged(contact);

   sendRejectFriendRequestToServer(friendUserId);

   removeFriendOrRequest(friendUserId);
}

void ChatClient::removeFriendOrRequest(const std::string &userId)
{
   onFriendRequestedRemove(userId);
}

void ChatClient::clearSearch()
{
   model_->clearSearch();
}

bool ChatClient::isFriend(const std::string &userId)
{
   return chatDb_->isContactExist(userId);
}

void ChatClient::onEditContactRequest(std::shared_ptr<Chat::Data> crecord)
{
   if (!crecord) {
      return;
   }

   auto contactRecord = crecord->mutable_contact_record();
   addOrUpdateContact(contactRecord->contact_id()
                      , contactRecord->status()
                      , contactRecord->display_name());
   getDataModel()->notifyContactChanged(crecord);
}

Chat::Data_ContactRecord ChatClient::getContact(const std::string &userId) const
{
   Chat::Data_ContactRecord contact;
   chatDb_->getContact(userId, &contact);

   return contact;
}

void ChatClient::onActionSearchUsers(const std::string &text)
{
   std::string pattern = text;

   QRegularExpressionMatch match = rx_email.match(QString::fromStdString(pattern));
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

void ChatClient::onMessageRead(std::shared_ptr<Chat::Data> message)
{
   if (message->message().sender_id() == model_->currentUser()) {
      return;
   }

   ChatUtils::messageFlagSet(message->mutable_message(), Chat::Data_Message_State_READ);
   chatDb_->updateMessageStatus(message->message().id(), message->message().state());
   model_->notifyMessageChanged(message);
   sendUpdateMessageState(message);
}


void ChatClient::onRoomMessageRead(std::shared_ptr<Chat::Data> message)
{
   ChatUtils::messageFlagSet(message->mutable_message(), Chat::Data_Message_State_READ);
   chatDb_->updateMessageStatus(message->message().id(), message->message().state());
   model_->notifyMessageChanged(message);
}

void ChatClient::onContactUpdatedByInput(std::shared_ptr<Chat::Data> crecord)
{
   addOrUpdateContact(crecord->contact_record().contact_id(),
                      crecord->contact_record().status(),
                      crecord->contact_record().display_name());
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

Chat::Data_Message_Encryption ChatClient::resolveMessageEncryption(std::shared_ptr<Chat::Data> message) const
{
   auto cNode = model_->findContactNode(message->message().receiver_id());

   if (!cNode) {
      return Chat::Data_Message_Encryption_IES;
   }

   switch (cNode->getOnlineStatus()) {
      case ChatContactElement::OnlineStatus::Online:
         return Chat::Data_Message_Encryption_AEAD;
      case ChatContactElement::OnlineStatus::Offline:
         return Chat::Data_Message_Encryption_IES;
   }

   // TODO: What default value we should return here? I have no idea.
   return Chat::Data_Message_Encryption_UNENCRYPTED;
}

void ChatClient::onRoomsLoaded(const std::vector<std::shared_ptr<Chat::Data>>& roomsList)

{
   for (const auto& room : roomsList) {
      model_->insertRoomObject(room);
   }
   emit RoomsInserted();
}

void ChatClient::onUserListChanged(Chat::Command command, const std::vector<std::string>& userList)
{
   for (const auto& user : userList) 
   {
      auto contact = model_->findContactNode(user);

      if (contact) {
         auto status = ChatContactElement::OnlineStatus::Offline;
         switch (command) {
            case Chat::COMMAND_REPLACE:
               status = ChatContactElement::OnlineStatus::Online;
               break;
            case Chat::COMMAND_ADD:
               status = ChatContactElement::OnlineStatus::Online;
               break;
            case Chat::COMMAND_DELETE:
               status = ChatContactElement::OnlineStatus::Offline;
               break;
         }

         contact->setOnlineStatus(status);
         model_->notifyContactChanged(contact->getDataObject());
      }
   }
}

void ChatClient::onMessageSent(const std::string& receiverId, const std::string& localId, const std::string& serverId)
{
   auto message = model_->findMessageItem(receiverId, localId);
   if (message) {
      message->mutable_message()->set_id(serverId);
      ChatUtils::messageFlagSet(message->mutable_message(), Chat::Data_Message_State_SENT);
      model_->notifyMessageChanged(message);
   } else {
      logger_->error("[ChatClient::{}] message not found: {}", __func__, localId);
   }
}

void ChatClient::onMessageStatusChanged(const std::string& chatId, const std::string& messageId, int newStatus)
{
   auto message = model_->findMessageItem(chatId, messageId);
   if (message) {
      message->mutable_message()->set_state(newStatus);
      model_->notifyMessageChanged(message);
   }
}

void ChatClient::onContactAccepted(const std::string& contactId)
{
   auto contactNode = model_->findContactNode(contactId);
   if (contactNode) {
      auto holdData = contactNode->getContactData();
      if (contactNode->getType() == ChatUIDefinitions::ChatTreeNodeType::ContactsRequestElement) {
         holdData->set_status(Chat::CONTACT_STATUS_ACCEPTED);
         contactNode->setOnlineStatus(ChatContactElement::OnlineStatus::Online);
         //model_->notifyContactChanged(data);
         model_->removeContactRequestNode(holdData->contact_id());
         model_->insertContactObject(contactNode->getDataObject()
                                     , contactNode->getOnlineStatus() == ChatContactElement::OnlineStatus::Online);

         retrieveUserMessages(holdData->contact_id());
      }
   }
}

void ChatClient::onContactRejected(const std::string& contactId)
{
   auto contactNode = model_->findContactNode(contactId);
   if (contactNode)
   {
      auto data = contactNode->getContactData();
      data->set_status(Chat::CONTACT_STATUS_REJECTED);
      contactNode->setOnlineStatus(ChatContactElement::OnlineStatus::Online);
      model_->notifyContactChanged(contactNode->getDataObject());
   }
}

void ChatClient::onFriendRequest(const std::string& userId, const std::string& contactId, const BinaryData& pk)
{
   auto contactNode = model_->findContactNode(contactId);
   if (contactNode) {
      auto holdData = contactNode->getContactData();
      if (contactNode->getType() == ChatUIDefinitions::ChatTreeNodeType::ContactsRequestElement) {
         holdData->set_status(Chat::CONTACT_STATUS_ACCEPTED);
         contactNode->setOnlineStatus(ChatContactElement::OnlineStatus::Online);
         //model_->notifyContactChanged(data);
         model_->removeContactRequestNode(holdData->contact_id());
         model_->insertContactObject(contactNode->getDataObject()
                                    , contactNode->getOnlineStatus() == ChatContactElement::OnlineStatus::Online);
      }
   } else {
      {
         auto contact = std::make_shared<Chat::Data>();
         auto d = contact->mutable_contact_record();
         d->set_user_id(userId);
         d->set_contact_id(contactId);
         d->set_status(Chat::CONTACT_STATUS_INCOMING);
         d->set_public_key(pk.toBinStr());

         model_->insertContactRequestObject(contact, true);
         addOrUpdateContact(contactId, Chat::CONTACT_STATUS_INCOMING);
      }

      emit NewContactRequest(contactId);
   }
}

void ChatClient::onContactRemove(const std::string& contactId)
{
   auto cNode = model_->findContactNode(contactId);
   if (!cNode) {
      return;
   }

   if (cNode->getType() == ChatUIDefinitions::ChatTreeNodeType::ContactsElement) {
      model_->removeContactNode(contactId);
   } else {
      model_->removeContactRequestNode(contactId);
   }

   emit ContactChanged();
}

void ChatClient::onCreateOutgoingContact(const std::string &contactId)
{
   //In base class this method calls addOrUpdateContact with Outgoing State
   BaseChatClient::onCreateOutgoingContact(contactId);

   auto citem = model_->findContactItem(contactId);

   if (citem && citem->contact_record().status() == Chat::ContactStatus::CONTACT_STATUS_OUTGOING_PENDING) {
       auto d = citem->mutable_contact_record();
       d->set_status(Chat::CONTACT_STATUS_OUTGOING);
       model_->notifyContactChanged(citem);
   } else if (!citem) {
      Chat::Data_ContactRecord contact;
      chatDb_->getContact(model_->currentUser(), &contact);

      auto record = std::make_shared<Chat::Data>();
      auto d = record->mutable_contact_record();
      d->set_user_id(model_->currentUser());
      d->set_contact_id(contactId);
      d->set_status(Chat::CONTACT_STATUS_OUTGOING);
      d->set_public_key(contact.public_key());
      d->set_public_key_timestamp(contact.public_key_timestamp());
      model_->insertContactObject(record);
   }

}

void ChatClient::onDMMessageReceived(const std::shared_ptr<Chat::Data>& messageData)
{
   model_->insertContactsMessage(messageData);

   if (messageData->message().sender_id() != currentUserId_)
      emit DMMessageReceived(messageData);
}

void ChatClient::onCRMessageReceived(const std::shared_ptr<Chat::Data> &messageData)
{
   model_->insertContactRequestMessage(messageData);
}

void ChatClient::onRoomMessageReceived(const std::shared_ptr<Chat::Data>& messageData)
{
   model_->insertRoomMessage(messageData);
}

void ChatClient::retrieveUserMessages(const std::string &userId)
{
   auto messages = chatDb_->getUserMessages(currentUserId_, userId);
   if (!messages.empty()) {
      for (auto &msg : messages) {
         if (msg->message().encryption() == Chat::Data_Message_Encryption_IES) {
            msg = decryptIESMessage(msg);
         }

         model_->insertContactsMessage(msg);
      }
   }
}

void ChatClient::loadRoomMessagesFromDB(const std::string& roomId)
{
   auto messages = chatDb_->getRoomMessages(roomId);

   if (!messages.empty()) {
      for (auto &msg : messages) {
         if (msg->message().encryption() == Chat::Data_Message_Encryption_IES) {
            msg = decryptIESMessage(msg);
         }

         auto existingMessage = model_->findMessageItem(roomId, msg->message().id());
         if (!existingMessage) {
            model_->insertRoomMessage(msg);
         }
      }
   }
}

void ChatClient::initMessage(Chat::Data *msg, const std::string &receiver)
{
   auto d = msg->mutable_message();
   d->set_sender_id(currentUserId_);
   d->set_receiver_id(receiver);
   d->set_id(CryptoPRNG::generateRandom(8).toHexStr());
   d->set_timestamp_ms(QDateTime::currentDateTimeUtc().toMSecsSinceEpoch());
}

void ChatClient::onContactListLoaded(const std::vector<std::shared_ptr<Chat::Data>>& remoteContacts)
{
   const auto localContacts = model_->getAllContacts();

   for (auto local : localContacts) {
      auto rit = std::find_if(remoteContacts.begin(), remoteContacts.end(),
                              [local](std::shared_ptr<Chat::Data> remote)
      {
         return local->contact_record().contact_id() == remote->contact_record().contact_id();
      });

      if (rit == remoteContacts.end()) {
         chatDb_->removeContact(local->contact_record().contact_id());
         model_->removeContactNode(local->contact_record().contact_id());
      }
   }

   for (auto remote : remoteContacts) {
      auto citem = model_->findContactItem(remote->contact_record().contact_id());
      if (!citem) {
         model_->insertContactObject(remote);
         //retrieveUserMessages(remote->getContactId());
      } else {
         citem->mutable_contact_record()->set_status(remote->contact_record().status());
         model_->notifyContactChanged(citem);
      }

      addOrUpdateContact(remote->contact_record().contact_id()
         , remote->contact_record().status(), remote->contact_record().display_name());
   }
}

void ChatClient::onSearchResult(const std::vector<std::shared_ptr<Chat::Data>>& userList)
{
   model_->insertSearchUserList(userList);

   emit SearchUserListReceived(userList, emailEntered_);
   emailEntered_ = false;
}

void ChatClient::updateMessageStateAndSave(const std::shared_ptr<Chat::Data>& message, const Chat::Data_Message_State& newState)
{
   Chat::Data_Message* msg = message->mutable_message();
   ChatUtils::messageFlagSet(msg, newState);

   const std::string messageId = msg->id();
   uint32_t state = msg->state();

   if (chatDb_->updateMessageStatus(messageId, state)) {
      std::string chatId = msg->sender_id() == currentUserId_
         ? msg->receiver_id() : msg->sender_id();

      auto modelMsg = model_->findMessageItem(chatId, messageId);
      if (modelMsg) {
         modelMsg->mutable_message()->set_state(state);
         model_->notifyMessageChanged(modelMsg);
      }
   }
   else {
      logger_->error("[ChatClient::{}] failed to update message state in DB: {} {}", __func__, messageId, state);
   }
}
