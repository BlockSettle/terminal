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


TreeItem * RootItem::resolveMessageTargetNode(TreeMessageNode * messageNode)
{
   if (!messageNode){
      return nullptr;
   }

   auto categoryIt = std::find_if(children_.begin(), children_.end(), [messageNode](TreeItem* child){
      return child->getAcceptType() == messageNode->getTargetParentType();
   });

   if (categoryIt != children_.end()) {

     TreeItem* target = (*categoryIt)->findSupportChild(messageNode);
      if (target) {
         return target;
      }
   }
   return nullptr;
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
            if (message && message->id().toStdString() == messgeId){
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

void RootItem::clearSearch()
{
   for (auto child : children_) {
      if (child->getAcceptType() == TreeItem::NodeType::SearchElement)
         child->deleteChildren();
   }
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
   } else {

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

TreeItem *RootItem::findCategoryNodeWith(TreeItem::NodeType type)
{
   auto found = std::find_if(children_.begin(), children_.end(),
                                       [type](TreeItem* child){
      return child->getAcceptType() == type;
   });

   if (found != children_.end()) {
      return *found;
   }
   return nullptr;
}


void RootItem::setCurrentUser(const std::string &currentUser)
{
   currentUser_ = currentUser;
}

void RootItem::notifyMessageChanged(std::shared_ptr<Chat::MessageData> message)
{
   QString chatId = message->senderId() == QString::fromStdString(currentUser())
                    ? message->receiverId()
                    : message->senderId();

   TreeItem* chatNode = findChatNode(chatId.toStdString());
   if (chatNode && chatNode->getAcceptType() == TreeItem::NodeType::MessageDataNode){
         for (auto child : chatNode->getChildren()){
            CategoryElement * elem = static_cast<CategoryElement*>(child);
            auto msg = std::dynamic_pointer_cast<Chat::MessageData>(elem->getDataObject());
            if (message->id() == msg->id()){
               emit itemChanged(elem);
            }
         }
         emit itemChanged(chatNode);
   }
}

void RootItem::notifyContactChanged(std::shared_ptr<Chat::ContactRecordData> contact)
{
   TreeItem* chatNode = findChatNode(contact->getContactId().toStdString());
   if (chatNode && chatNode->getType() == TreeItem::NodeType::ContactsElement){
      emit itemChanged(chatNode);
   }
}

bool CategoryElement::updateNewItemsFlag()
{
   if (acceptType_ != NodeType::MessageDataNode) {
      return false;
   }
   //Reset flag
   newItemsFlag_ = false;


   for (const auto child : children_){
      auto messageNode = static_cast<TreeMessageNode*>(child);

      if (!messageNode) {
         return false;
      }

      auto message = messageNode->getMessage();
      const RootItem * root = static_cast<const RootItem*>(recursiveRoot());
      if (message
          && !message->testFlag(Chat::MessageData::State::Read)
          && root->currentUser() != message->senderId().toStdString()) {
         newItemsFlag_ = true;
         break; //If first is found, no reason to continue
      }
   }
   return newItemsFlag_;
}

bool CategoryElement::getNewItemsFlag() const
{
   return newItemsFlag_;
}

void CategoryElement::setNewItemsFlag(bool newItemsFlag)
{
   newItemsFlag_ = newItemsFlag;
}