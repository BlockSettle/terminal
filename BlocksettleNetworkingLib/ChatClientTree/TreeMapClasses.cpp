#include "TreeMapClasses.h"
#include "TreeObjects.h"
#include <algorithm>

bool RootItem::insertRoomObject(std::shared_ptr<Chat::RoomData> data){
   TreeItem* candidate =  new ChatRoomElement(data);
   bool res = insertNode(candidate);
   if (!res) {
      delete  candidate;
   }
   return  res;
}

bool RootItem::insertContactObject(std::shared_ptr<Chat::ContactRecordData> data, bool isOnline){
   ChatContactElement* candidate = new ChatContactElement(data);
   candidate->setOnlineStatus(isOnline
                              ?ChatContactElement::OnlineStatus::Online
                              :ChatContactElement::OnlineStatus::Offline);
   bool res = insertNode(candidate);
   return  res;
}

bool RootItem::insertGeneralUserObject(std::shared_ptr<Chat::UserData> data)
{
   TreeItem* candidate = new ChatUserElement(data);
   bool res = insertNode(candidate);
   if (!res) {
      delete  candidate;
   }
   return  res;
}

bool RootItem::insertSearchUserObject(std::shared_ptr<Chat::UserData> data)
{
   TreeItem* candidate = new ChatSearchElement(data);
   bool res = insertNode(candidate);
   if (!res) {
      delete  candidate;
   }
   return  res;
}

bool RootItem::insertRoomMessage(std::shared_ptr<Chat::MessageData> message)
{
   TreeMessageNode * messageNode = new TreeMessageNode(TreeItem::NodeType::RoomsElement, message);
   bool res = insertMessageNode(messageNode);
   if (!res){
      delete messageNode;
   }
   return res;
}

bool RootItem::insertContactsMessage(std::shared_ptr<Chat::MessageData> message)
{
   TreeMessageNode * messageNode = new TreeMessageNode(TreeItem::NodeType::ContactsElement, message);
   bool res = insertMessageNode(messageNode);
   if (!res){
      delete messageNode;
   }
   return res;
}

TreeItem* RootItem::findChatNode(const std::string &chatId)
{
   for (auto child : children_){ // through all categories
      switch (child->getAcceptType()) {
         case TreeItem::NodeType::RoomsElement:
         case TreeItem::NodeType::ContactsElement:
            for (auto cchild : child->getChildren()){
               auto data = static_cast<CategoryElement*>(cchild)->getDataObject();
               switch (data->getType()){
                  case Chat::DataObject::Type::RoomData:{
                     auto room = std::dynamic_pointer_cast<Chat::RoomData>(data);
                     if (room->getId().toStdString() == chatId){
                        return cchild;
                     }
                  }
                     break;
                  case Chat::DataObject::Type::ContactRecordData: {
                     auto contact = std::dynamic_pointer_cast<Chat::ContactRecordData>(data);
                     if (contact->getContactId().toStdString() == chatId){
                        return cchild;
                     }
                  }
                     break;
                  default:
                     break;

               }
            }
            break;
         default:
            break;

      }
   }
   return nullptr;
}

std::vector<std::shared_ptr<Chat::ContactRecordData> > RootItem::getAllContacts()
{
   std::vector<std::shared_ptr<Chat::ContactRecordData>> contacts;

   for (auto child : children_){ // through all categories
      switch (child->getAcceptType()) {
         case TreeItem::NodeType::ContactsElement:
            for (auto cchild : child->getChildren()){
               auto data = static_cast<CategoryElement*>(cchild)->getDataObject();
               if (data->getType() == Chat::DataObject::Type::ContactRecordData) {
                  auto contact = std::dynamic_pointer_cast<Chat::ContactRecordData>(data);
                  contacts.push_back(contact);
               }
            }
            break;
         default:
            break;

      }
   }
   return  contacts;
}

bool RootItem::removeContactNode(const std::string &contactId)
{
   for (auto child : children_){ // through all categories
      switch (child->getAcceptType()) {
         case TreeItem::NodeType::ContactsElement:
            for (auto cchild : child->getChildren()){
               auto data = static_cast<CategoryElement*>(cchild)->getDataObject();
               if (data->getType() == Chat::DataObject::Type::ContactRecordData) {
                  auto contact = std::dynamic_pointer_cast<Chat::ContactRecordData>(data);
                  if (contact->getContactId().toStdString() == contactId) {
                     child->removeChild(cchild);
                     return true;
                  }
               }
            }
            break;
         default:
            break;

      }
   }
   return false;
}

std::shared_ptr<Chat::ContactRecordData> RootItem::findContactItem(const std::string &contactId)
{
   ChatContactElement* contactNode = findContactNode(contactId);
   if (contactNode){
      return contactNode->getContactData();
   }
   return  nullptr;
}

ChatContactElement *RootItem::findContactNode(const std::string &contactId)
{
   TreeItem* chatNode = findChatNode(contactId);
   if (chatNode && chatNode->getType() == TreeItem::NodeType::ContactsElement){
      return static_cast<ChatContactElement*> (chatNode);
   }
   return nullptr;
}

std::shared_ptr<Chat::MessageData> RootItem::findMessageItem(const std::string &chatId, const std::string &messgeId)
{
   TreeItem* chatNode = findChatNode(chatId);
   if (chatNode && chatNode->getAcceptType() == TreeItem::NodeType::MessageDataNode){
         for (auto child : chatNode->getChildren()){
            auto message = std::dynamic_pointer_cast<Chat::MessageData>(static_cast<CategoryElement*>(child)->getDataObject());
            if (message && message->getId().toStdString() == messgeId){
               return message;
            }
         }
   }
   return  nullptr;
}

void RootItem::clear()
{
   for (auto child : children_) {
      child->deleteChildren();
   }
   currentUser_.clear();
}

std::string RootItem::currentUser() const
{
   return currentUser_;
}

bool RootItem::insertMessageNode(TreeMessageNode * messageNode)
{
   //assert(targetElement >= NodeType::RoomsElement && targetElement <= NodeType::AllUsersElement);
      auto categoryIt = std::find_if(children_.begin(), children_.end(), [messageNode](TreeItem* child){
         return child->getAcceptType() == messageNode->getTargetParentType();
      });

      if (categoryIt != children_.end()) {

        TreeItem* target = (*categoryIt)->findSupportChild(messageNode);
         if (target) {
            bool res = target->insertItem(messageNode);
            if (res) {
               emit itemChanged(target);
            }
            return res;
         }
         return false;
      }
      return false;


}

bool RootItem::insertNode(TreeItem * item)
{
   TreeItem * supportChild = findSupportChild(item);
   if (supportChild) {
      return supportChild->insertItem(item);
   }
   return false;

//   auto it = std::find_if(children_.begin(), children_.end(), [item](TreeItem* child){
//      return child->getType() == item->getTargetParentType()
//             && child->getAcceptType() == item->getType();
//   });

//   if (it != children_.end()){
//      return (*it)->insertItem(item);
//   }
//   return false;
}


void RootItem::setCurrentUser(const std::string &currentUser)
{
   currentUser_ = currentUser;
}

void RootItem::notifyMessageChanged(std::shared_ptr<Chat::MessageData> message)
{
   QString chatId = message->getSenderId() == QString::fromStdString(currentUser())
                    ? message->getReceiverId()
                    : message->getSenderId();

   TreeItem* chatNode = findChatNode(chatId.toStdString());
   if (chatNode && chatNode->getAcceptType() == TreeItem::NodeType::MessageDataNode){
         for (auto child : chatNode->getChildren()){
            CategoryElement * elem = static_cast<CategoryElement*>(child);
            auto msg = std::dynamic_pointer_cast<Chat::MessageData>(elem->getDataObject());
            if (message->getId() == msg->getId()){
               emit itemChanged(elem);
            }
         }
   }
}

void RootItem::notifyContactChanged(std::shared_ptr<Chat::ContactRecordData> contact)
{
   TreeItem* chatNode = findChatNode(contact->getContactId().toStdString());
   if (chatNode && chatNode->getType() == TreeItem::NodeType::ContactsElement){
      emit itemChanged(chatNode);
   }
}
