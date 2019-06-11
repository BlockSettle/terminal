#include "TreeObjects.h"

std::shared_ptr<Chat::RoomData> ChatRoomElement::getRoomData() const
{
   return std::dynamic_pointer_cast<Chat::RoomData>(getDataObject());
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
         auto room = std::dynamic_pointer_cast<Chat::RoomData>(getDataObject());
         if (room){
            auto mNode = dynamic_cast<const TreeMessageNode*>(item);
            if (mNode){
//             bool forCurrentUser = (mNode->getMessage()->getSenderId().toStdString() == user
//                             || mNode->getMessage()->getReceiverId().toStdString() == user);
               bool forThisElement =    /*mNode->getMessage()->getSenderId() == room->getId()
                                     || */mNode->getMessage()->receiverId() == room->getId();

               byData = forThisElement;
            }
         }
      }

   }

   return byTypes && byData;
}

std::shared_ptr<Chat::ContactRecordData> ChatContactElement::getContactData() const
{
   return std::dynamic_pointer_cast<Chat::ContactRecordData>(getDataObject());
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
      switch (data->getType()) {
         case Chat::DataObject::Type::MessageData: {
            auto message = std::dynamic_pointer_cast<Chat::MessageData>(data);
            if (message) {
               bool forCurrentUser = (message->senderId().toStdString() == user
                                      || message->receiverId().toStdString() == user);
               bool forThisElement = forCurrentUser &&
                                     (  message->senderId() == contact->getContactId()
                                      || message->receiverId() == contact->getContactId());

               byData = forThisElement;
            }
         }
         break;
         case Chat::DataObject::Type::OTCRequestData: {
            auto otcRequest = std::dynamic_pointer_cast<Chat::OTCRequestData>(data);
            if (otcRequest) {
               bool forCurrentUser = (otcRequest->requestorId() == user
                                      || otcRequest->targetId() == user);

               bool forThisElement = forCurrentUser &&
                                     (otcRequest->requestorId() == contact->getContactId().toStdString()
                                      || otcRequest->targetId() == contact->getContactId().toStdString());
               byData = forThisElement;
            }
         }
         break;
         case Chat::DataObject::Type::OTCResponseData: {
            auto otcResponse = std::dynamic_pointer_cast<Chat::OTCResponseData>(data);
            if (otcResponse) {
               bool forCurrentUser = (otcResponse->responderId() == user
                                      || otcResponse->requestorId() == user);

               bool forThisElement = forCurrentUser &&
                                     (otcResponse->responderId() == contact->getContactId().toStdString()
                                      || otcResponse->requestorId() == contact->getContactId().toStdString());
               byData = forThisElement;
            }
         }
         break;
         case Chat::DataObject::Type::OTCUpdateData: {
            auto otcUpdate = dynamic_cast<const Chat::OTCUpdateData*>(item);
            if (otcUpdate && isOTCResponsePresented()) {
               bool forThisElement = getActiveOtcResponse()->serverResponseId() ==
                                     otcUpdate->serverResponseId();
               byData = forThisElement;
            }
         }
         break;
         default:
            break;
      }
   }

   return byTypes && byData;
}

bool ChatContactElement::isHaveActiveOTC() const
{
   return activeOtcRequest_ != nullptr;
}

bool ChatContactElement::isOTCResponsePresented() const
{
   return activeOtcResponse_ != nullptr;
}

std::shared_ptr<Chat::OTCRequestData> ChatContactElement::getActiveOtcRequest() const
{
   return activeOtcRequest_;
}

void ChatContactElement::setActiveOtcRequest(const std::shared_ptr<Chat::OTCRequestData> &activeOtcRequest)
{
   activeOtcRequest_ = activeOtcRequest;
}

void ChatContactElement::setActiveOtcResponse(const std::shared_ptr<Chat::OTCResponseData> &activeOtcResponse)
{
   activeOtcResponse_ = activeOtcResponse;
}

std::shared_ptr<Chat::OTCResponseData> ChatContactElement::getActiveOtcResponse() const
{
   return activeOtcResponse_;
}

ChatContactElement::OnlineStatus ChatContactElement::getOnlineStatus() const
{
   return onlineStatus_;
}

void ChatContactElement::setOnlineStatus(const OnlineStatus &onlineStatus)
{
   onlineStatus_ = onlineStatus;
}

std::shared_ptr<Chat::UserData> ChatUserElement::getUserData() const
{
   return std::dynamic_pointer_cast<Chat::UserData>(getDataObject());;
}

std::shared_ptr<Chat::UserData> ChatSearchElement::getUserData() const
{
   return std::dynamic_pointer_cast<Chat::UserData>(getDataObject());
}

std::shared_ptr<Chat::OTCResponseData> OTCSentResponseElement::getOTCResponse() const
{
   return std::dynamic_pointer_cast<Chat::OTCResponseData>(getDataObject());
}

std::string OTCSentResponseElement::otcId() const
{
   return getOTCResponse()->serverResponseId();
}

std::shared_ptr<Chat::OTCResponseData> OTCReceivedResponseElement::getOTCResponse() const
{
   return std::dynamic_pointer_cast<Chat::OTCResponseData>(getDataObject());
}

std::string OTCReceivedResponseElement::otcId() const
{
   return getOTCResponse()->serverResponseId();
}

std::shared_ptr<Chat::DataObject> DisplayableDataNode::getDataObject() const
{
   return data_;
}
