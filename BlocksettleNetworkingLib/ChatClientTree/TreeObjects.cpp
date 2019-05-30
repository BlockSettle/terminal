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
            auto mNode = static_cast<const TreeMessageNode*>(item);
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
   bool byData = false;
   if (byTypes) {
      const RootItem* root = nullptr;
      const TreeItem* try_root = recursiveRoot();
      if (try_root->getType() == ChatUIDefinitions::ChatTreeNodeType::RootNode) {
         root = static_cast<const RootItem*>(try_root);
      }
      if (root) {
         std::string user = root->currentUser();
         auto contact = std::dynamic_pointer_cast<Chat::ContactRecordData>(getDataObject());
         if (contact) {
            auto mNode = static_cast<const TreeMessageNode*>(item);
            if (mNode) {
               bool forCurrentUser = (mNode->getMessage()->senderId().toStdString() == user
                                   || mNode->getMessage()->receiverId().toStdString() == user);
               bool forThisElement = forCurrentUser &&
                                     (  mNode->getMessage()->senderId() == contact->getContactId()
                                      ||mNode->getMessage()->receiverId() == contact->getContactId());

               byData = forThisElement;
            }
         }
      }

   }

   return byTypes && byData;
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
