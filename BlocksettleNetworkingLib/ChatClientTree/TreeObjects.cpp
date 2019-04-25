#include "TreeObjects.h"

std::shared_ptr<Chat::RoomData> ChatRoomElement::getRoomData() const
{
   auto room = std::dynamic_pointer_cast<Chat::RoomData>(getDataObject());
   if (room) {
      return room;
   }
   return nullptr;
}

bool ChatRoomElement::isSupported(TreeItem *item) const
{
   bool byTypes = CategoryElement::isSupported(item);
   bool byData = false;
   if (byTypes) {
      const RootItem* root = nullptr;
      const TreeItem* try_root = recursiveRoot();
      if (try_root->getType() == TreeItem::NodeType::RootNode) {
         root = static_cast<const RootItem*>(try_root);
      }
      if (root) {
         std::string user = root->currentUser();
         auto room = std::dynamic_pointer_cast<Chat::RoomData>(getDataObject());
         if (room){
            TreeMessageNode * mNode = static_cast<TreeMessageNode*>(item);
            if (mNode){
//             bool forCurrentUser = (mNode->getMessage()->getSenderId().toStdString() == user
//                             || mNode->getMessage()->getReceiverId().toStdString() == user);
               bool forThisElement =    /*mNode->getMessage()->getSenderId() == room->getId()
                                     || */mNode->getMessage()->getReceiverId() == room->getId();

               byData = forThisElement;
            }
         }
      }

   }

   return byTypes && byData;
}

std::shared_ptr<Chat::ContactRecordData> ChatContactElement::getContactData() const
{
   auto crecord = std::dynamic_pointer_cast<Chat::ContactRecordData>(getDataObject());
   if (crecord) {
      return crecord;
   }
   return nullptr;
}

bool ChatContactElement::isSupported(TreeItem *item) const
{
   bool byTypes = CategoryElement::isSupported(item);
   bool byData = false;
   if (byTypes) {
      const RootItem* root = nullptr;
      const TreeItem* try_root = recursiveRoot();
      if (try_root->getType() == TreeItem::NodeType::RootNode) {
         root = static_cast<const RootItem*>(try_root);
      }
      if (root) {
         std::string user = root->currentUser();
         auto contact = std::dynamic_pointer_cast<Chat::ContactRecordData>(getDataObject());
         if (contact) {
            TreeMessageNode * mNode = static_cast<TreeMessageNode*>(item);
            if (mNode) {
               bool forCurrentUser = (mNode->getMessage()->getSenderId().toStdString() == user
                                   || mNode->getMessage()->getReceiverId().toStdString() == user);
               bool forThisElement = forCurrentUser &&
                                     (  mNode->getMessage()->getSenderId() == contact->getContactId()
                                      ||mNode->getMessage()->getReceiverId() == contact->getContactId());

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
   auto user = std::dynamic_pointer_cast<Chat::UserData>(getDataObject());
   if (user) {
      return user;
   }
   return nullptr;
}
