#include "TreeObjects.h"

Chat::Data_Room* ChatRoomElement::getRoomData() const
{
   if (getDataObject()->has_room()) {
      return getDataObject()->mutable_room();
   }
   return nullptr;
}

bool ChatRoomElement::isChildSupported(const TreeItem *item) const
{
   bool byTypes = CategoryElement::isChildSupported(item);
   bool byData = false;
   if (byTypes) {
      const RootItem* root = nullptr;
      const TreeItem* try_root = recursiveRoot();
      if (try_root->getType() == ChatUIDefinitions::ChatTreeNodeType::RootNode) {
         root = static_cast<const RootItem*>(try_root);
      }
      if (root) {
         std::string user = root->currentUser();
         auto room = getDataObject();
         if (room && room->has_room()) {
            auto mNode = dynamic_cast<const TreeMessageNode*>(item);
            if (mNode) {
//             bool forCurrentUser = (mNode->getMessage()->getSenderId().toStdString() == user
//                             || mNode->getMessage()->getReceiverId().toStdString() == user);
               bool forThisElement =    /*mNode->getMessage()->getSenderId() == room->getId()
                                     || */mNode->getMessage()->message().receiver_id() == room->room().id();

               byData = forThisElement;
            }
         }
      }

   }

   return byTypes && byData;
}

Chat::Data_ContactRecord *ChatContactElement::getContactData() const
{
   if (getDataObject()->has_contact_record()) {
      return getDataObject()->mutable_contact_record();
   }

   return nullptr;
}

ChatContactElement::OnlineStatus ChatContactElement::getOnlineStatus() const
{
   return onlineStatus_;
}

void ChatContactElement::setOnlineStatus(const OnlineStatus &onlineStatus)
{
   onlineStatus_ = onlineStatus;
}

bool ChatContactElement::isChildSupported(const TreeItem *item) const
{
   bool byTypes = CategoryElement::isChildSupported(item);

   if (!byTypes) {
      return byTypes;
   }

   const RootItem* root = nullptr;
   const TreeItem* try_root = recursiveRoot();
   if (try_root->getType() == ChatUIDefinitions::ChatTreeNodeType::RootNode) {
      root = static_cast<const RootItem*>(try_root);
   }

   if (!root) {
      return false;
   }

   bool byData = false;
   std::string user = root->currentUser();
   auto contact = getContactData();
   auto mNode = dynamic_cast<const DisplayableDataNode*>(item);
   if (contact && mNode) {
      auto data = mNode->getDataObject();
      if (data->has_message()) {
         const auto& message = data->message();
         bool forCurrentUser = (message.sender_id() == user
                                || message.receiver_id() == user);
         bool forThisElement = forCurrentUser &&
                               (  message.sender_id() == contact->contact_id()
                                || message.receiver_id() == contact->contact_id());

         byData = forThisElement;
      }

         // XXXOTC
         // case Chat::Data::Type::OTCRequestData: {
         //    if (data->message().has_request()) {
         //       bool forCurrentUser = (otcRequest->senderId() == user
         //                              || otcRequest->receiverId() == user);

         //       bool forThisElement = forCurrentUser &&
         //                             (otcRequest->senderId() == contact->getContactId()
         //                              || otcRequest->receiverId() == contact->getContactId());
         //       byData = forThisElement;
         //    }
         // }
         // break;
         // case Chat::Data::Type::OTCResponseData: {
         //    if (data->message().has_response()) {
         //       bool forCurrentUser = (otcResponse->responderId() == user
         //                              || otcResponse->requestorId() == user);

         //       bool forThisElement = forCurrentUser &&
         //                             (otcResponse->responderId() == contact->getContactId().toStdString()
         //                              || otcResponse->requestorId() == contact->getContactId().toStdString());
         //       byData = forThisElement;
         //    }
         // }
         // break;
         // case Chat::Data::Type::OTCUpdateData: {
         //    if (data->message().has_update() && isOTCResponsePresented()) {
         //       bool forThisElement = getActiveOtcResponse()->serverResponseId() ==
         //                             otcUpdate->serverResponseId();
         //       byData = forThisElement;
         //    }
         // }
         // break;
   }

   return byTypes && byData;
}

bool ChatContactCompleteElement::OTCTradingStarted() const
{
   return otcRequest_ != nullptr;
}

bool ChatContactCompleteElement::isOTCRequestor() const
{
   return otcRequest_->direction() == Chat::Data_Direction_SENT;
}

bool ChatContactCompleteElement::haveUpdates() const
{
   return otcLastUpdate_ != nullptr;
}

bool ChatContactCompleteElement::haveResponse() const
{
   return otcResponse_ != nullptr;
}



Chat::Data_User* ChatUserElement::getUserData() const
{
   auto data = getDataObject();
   if (data->has_user()) {
      return data->mutable_user();
   }
   return nullptr;
}

Chat::Data_User *ChatSearchElement::getUserData() const
{
   auto data = getDataObject();
   if (data->has_user()) {
      return data->mutable_user();
   }
   return nullptr;
}


std::shared_ptr<Chat::Data> DisplayableDataNode::getDataObject() const
{
   return data_;
}

void ChatContactCompleteElement::onChildAdded(TreeItem* item)
{
   if (item->getType() == ChatUIDefinitions::ChatTreeNodeType::MessageDataNode) {
      auto messageNode = dynamic_cast<TreeMessageNode*>(item);
      if (messageNode) {
         auto msg = messageNode->getMessage();
         auto messageDirection = msg->direction();

         bool isOtc = msg->message().otc_case() != Chat::Data_Message::OTC_NOT_SET;
         if (messageDirection != Chat::Data_Direction_NOT_SET && isOtc && !msg->message().loaded_from_history()) {
            processOTCMessage(msg);
         }
      }
   }
}

void ChatContactCompleteElement::processOTCMessage(const std::shared_ptr<Chat::Data>& messageData)
{
   assert(messageData->has_message());

   if (messageData->message().has_otc_request()) {
      otcRequest_ = messageData;
      return;
   }

   if (messageData->message().has_otc_response()) {
      otcResponse_ = messageData;
      return;
   }

   if (messageData->message().has_otc_update()) {
      otcLastUpdate_ = messageData;
      return;
   }

   if (messageData->message().has_otc_close_trading()) {
      cleanupTrading();
      return;
   }
}

std::shared_ptr<Chat::Data> ChatContactCompleteElement::getOTCRequest() const
{
   return otcRequest_;
}

std::shared_ptr<Chat::Data> ChatContactCompleteElement::getOTCResponse() const
{
   return otcResponse_;
}

std::shared_ptr<Chat::Data> ChatContactCompleteElement::getLastOTCUpdate() const
{
   return otcLastUpdate_;
}

void ChatContactCompleteElement::cleanupTrading()
{
   otcRequest_ = nullptr;
   otcResponse_ = nullptr;
   otcLastUpdate_ = nullptr;
}
