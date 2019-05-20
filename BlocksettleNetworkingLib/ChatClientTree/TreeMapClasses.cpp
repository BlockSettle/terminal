#include "TreeMapClasses.h"

#include "TreeObjects.h"

#include <algorithm>
#include <QDebug>

bool RootItem::insertRoomObject(std::shared_ptr<Chat::RoomData> data)
{
   TreeItem* candidate =  new ChatRoomElement(data);
   bool res = insertNode(candidate);
   if (!res) {
      delete  candidate;
   }
   return  res;
}

bool RootItem::insertContactObject(std::shared_ptr<Chat::ContactRecordData> data, bool isOnline)
{
   ChatContactElement* candidate = new ChatContactElement(data);
   candidate->setOnlineStatus(isOnline
                              ? ChatContactElement::OnlineStatus::Online
                              : ChatContactElement::OnlineStatus::Offline);
   bool res = insertNode(candidate);
   if (!res) {
      delete candidate;
   }

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

   for (auto categoryGroup : children_) {
      if (categoryGroup->isChildTypeSupported(messageNode->targetParentType_)) {
         return categoryGroup->findSupportChild(messageNode);
      }
   }

   return nullptr;
}

TreeItem* RootItem::findChatNode(const std::string &chatId)
{
   for (auto child : children_){ // through all categories
      if ( child->isChildTypeSupported(ChatUIDefinitions::ChatTreeNodeType::RoomsElement)
        || child->isChildTypeSupported(ChatUIDefinitions::ChatTreeNodeType::ContactsElement)) {
         for (auto cchild : child->getChildren()) {
            auto data = static_cast<CategoryElement*>(cchild)->getDataObject();
            switch (data->getType()) {
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
      }
   }
   return nullptr;
}

std::vector<std::shared_ptr<Chat::ContactRecordData> > RootItem::getAllContacts()
{
   std::vector<std::shared_ptr<Chat::ContactRecordData>> contacts;

   for (auto child : children_){ // through all categories
      if (child->isChildTypeSupported(ChatUIDefinitions::ChatTreeNodeType::ContactsElement)) {
         for (auto cchild : child->getChildren()){
            auto data = static_cast<CategoryElement*>(cchild)->getDataObject();
            if (data->getType() == Chat::DataObject::Type::ContactRecordData) {
               auto contact = std::dynamic_pointer_cast<Chat::ContactRecordData>(data);
               contacts.push_back(contact);
            }
         }
      }
   }
   return  contacts;
}

bool RootItem::removeContactNode(const std::string &contactId)
{
   for (auto child : children_){ // through all categories
      if (child->isChildTypeSupported(ChatUIDefinitions::ChatTreeNodeType::ContactsElement)) {
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
   if (chatNode && chatNode->getType() == ChatUIDefinitions::ChatTreeNodeType::ContactsElement){
      return static_cast<ChatContactElement*> (chatNode);
   }
   return nullptr;
}

std::shared_ptr<Chat::MessageData> RootItem::findMessageItem(const std::string &chatId, const std::string &messgeId)
{
   TreeItem* chatNode = findChatNode(chatId);
   if (chatNode && chatNode->isChildTypeSupported(ChatUIDefinitions::ChatTreeNodeType::MessageDataNode)) {
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
      if (child->isChildTypeSupported(ChatUIDefinitions::ChatTreeNodeType::SearchElement))
         child->deleteChildren();
   }
}

std::string RootItem::currentUser() const
{
   return currentUser_;
}

bool RootItem::insertMessageNode(TreeMessageNode * messageNode)
{
   for (auto categoryGroup : children_ ) {
      if (categoryGroup->isChildTypeSupported(messageNode->targetParentType_)) {
         auto targetElement = categoryGroup->findSupportChild(messageNode);
         if (targetElement != nullptr) {
            if (targetElement->insertItem(messageNode)) {
               emit itemChanged(targetElement);
               return true;
            }
            break;
         }
      }
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
}

TreeItem *RootItem::findCategoryNodeWith(ChatUIDefinitions::ChatTreeNodeType type)
{
   auto found = std::find_if(children_.begin(), children_.end(),
                                       [type](TreeItem* child){
      return child->isChildTypeSupported(type);
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
   if (message) {
      QString chatId = message->senderId() == QString::fromStdString(currentUser())
                       ? message->receiverId()
                       : message->senderId();

      TreeItem* chatNode = findChatNode(chatId.toStdString());
      if (chatNode == nullptr) {
         chatId = message->receiverId();
         chatNode = findChatNode(chatId.toStdString());
      }

      if (chatNode && chatNode->isChildTypeSupported(ChatUIDefinitions::ChatTreeNodeType::MessageDataNode)) {
         for (auto child : chatNode->getChildren()) {
            CategoryElement * elem = static_cast<CategoryElement*>(child);
            auto msg = std::dynamic_pointer_cast<Chat::MessageData>(elem->getDataObject());
            if (message->id() == msg->id()) {
               emit itemChanged(elem);
            }
         }
         emit itemChanged(chatNode);
      }
   }
}

void RootItem::notifyContactChanged(std::shared_ptr<Chat::ContactRecordData> contact)
{
   TreeItem* chatNode = findChatNode(contact->getContactId().toStdString());
   if (chatNode && chatNode->getType() == ChatUIDefinitions::ChatTreeNodeType::ContactsElement){
      emit itemChanged(chatNode);
   }
}

bool CategoryElement::updateNewItemsFlag()
{
   if (!isChildTypeSupported(ChatUIDefinitions::ChatTreeNodeType::MessageDataNode)) {
      return false;
   }
   //Reset flag
   newItemsFlag_ = false;


   for (const auto child : children_) {
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

// insert channel for response that client send to OTC requests
bool RootItem::insertOTCSentResponseObject(const std::string& otcId)
{
   auto otcRequestNode = new OTCSentResponseElement(otcId);
   bool insertResult = insertNode(otcRequestNode);
   if (!insertResult) {
      delete otcRequestNode;
   }

   qDebug() << "Sent response added";
   return insertResult;
}

// insert channel for response client receive for own OTC
bool RootItem::insertOTCReceivedResponseObject(const std::string& otcId)
{
   auto otcRequestNode = new OTCReceivedResponseElement(otcId);
   bool insertResult = insertNode(otcRequestNode);
   if (!insertResult) {
      delete otcRequestNode;
   }

   qDebug() << "Received response added";

   return insertResult;
}

