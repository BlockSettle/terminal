#include "TreeMapClasses.h"

#include "TreeObjects.h"
#include "ChatProtocol/ChatUtils.h"

#include <algorithm>

bool RootItem::insertRoomObject(std::shared_ptr<Chat::Data> data)
{
   assert(data->has_room());
   TreeItem* candidate =  new ChatRoomElement(data);
   bool res = insertNode(candidate);
   if (!res) {
      delete  candidate;
   }
   return  res;
}

bool RootItem::insertContactObject(std::shared_ptr<Chat::Data> data, bool isOnline)
{
   assert(data->has_contact_record());
   ChatContactElement* candidate = new ChatContactCompleteElement(data);
   candidate->setOnlineStatus(isOnline
                              ? ChatContactElement::OnlineStatus::Online
                              : ChatContactElement::OnlineStatus::Offline);
   bool res = insertNode(candidate);
   if (!res) {
      delete candidate;
   }

   return  res;
}

bool RootItem::insertContactRequestObject(std::shared_ptr<Chat::Data> data, bool isOnline)
{
   assert(data->has_contact_record());
   ChatContactElement* candidate = new ChatContactRequestElement(data);
   candidate->setOnlineStatus(isOnline
                              ? ChatContactElement::OnlineStatus::Online
                              : ChatContactElement::OnlineStatus::Offline);
   bool res = insertNode(candidate);
   if (!res) {
      delete candidate;
   }

   return  res;
}

bool RootItem::insertGeneralUserObject(std::shared_ptr<Chat::Data> data)
{
   assert(data->has_user());
   TreeItem* candidate = new ChatUserElement(data);
   bool res = insertNode(candidate);
   if (!res) {
      delete  candidate;
   }
   return  res;
}

bool RootItem::insertSearchUserObject(std::shared_ptr<Chat::Data> data)
{
   assert(data->has_user());
   TreeItem* candidate = new ChatSearchElement(data);
   bool res = insertNode(candidate);
   if (!res) {
      delete  candidate;
   }
   return  res;
}


TreeItem * RootItem::resolveMessageTargetNode(DisplayableDataNode * messageNode)
{
   if (!messageNode) {
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
   for (auto child : children_) { // through all categories
      if ( child->isChildTypeSupported(ChatUIDefinitions::ChatTreeNodeType::RoomsElement)
        || child->isChildTypeSupported(ChatUIDefinitions::ChatTreeNodeType::ContactsElement)
        || child->isChildTypeSupported(ChatUIDefinitions::ChatTreeNodeType::ContactsRequestElement)) {
         for (auto cchild : child->getChildren()) {
            auto data = static_cast<CategoryElement*>(cchild)->getDataObject();

            if (data->has_room()) {
               if (data->room().id() == chatId) {
                  return cchild;
               }
            }

            if (data->has_contact_record()) {
               if (data->contact_record().contact_id() == chatId) {
                  return cchild;
               }
            }
         }
      }
   }

   return nullptr;
}

std::vector<std::shared_ptr<Chat::Data> > RootItem::getAllContacts()
{
   std::vector<std::shared_ptr<Chat::Data>> contacts;

   for (auto child : children_) { // through all categories
      if (child->isChildTypeSupported(ChatUIDefinitions::ChatTreeNodeType::ContactsElement)) {
         for (auto cchild : child->getChildren()) {
            auto data = static_cast<CategoryElement*>(cchild)->getDataObject();
            if (data->has_contact_record()) {
               contacts.push_back(data);
            }
         }
      }
   }
   return  contacts;
}

bool RootItem::removeContactNode(const std::string &contactId)
{
   for (auto child : children_) { // through all categories
      if (child->isChildTypeSupported(ChatUIDefinitions::ChatTreeNodeType::ContactsElement)) {
         for (auto cchild : child->getChildren()) {
            auto data = static_cast<CategoryElement*>(cchild)->getDataObject();
            if (data->has_contact_record()) {
               if (data->contact_record().contact_id() == contactId) {
                  child->removeChild(cchild);
                  return true;
               }
            }
         }
      }
   }
   return false;
}

bool RootItem::removeContactRequestNode(const std::string &contactId)
{
   for (auto child : children_) { // through all categories
      if (child->isChildTypeSupported(ChatUIDefinitions::ChatTreeNodeType::ContactsRequestElement)) {
         for (auto cchild : child->getChildren()) {
            auto data = static_cast<CategoryElement*>(cchild)->getDataObject();
            if (data->has_contact_record()) {
               if (data->contact_record().contact_id() == contactId) {
                  child->removeChild(cchild);
                  return true;
               }
            }
         }
      }
   }
   return false;
}

std::shared_ptr<Chat::Data> RootItem::findContactItem(const std::string &contactId)
{
   ChatContactElement* contactNode = findContactNode(contactId);
   if (contactNode) {
      return contactNode->getDataObject();
   }
   return  nullptr;
}

ChatContactElement *RootItem::findContactNode(const std::string &contactId)
{
   TreeItem* chatNode = findChatNode(contactId);
   if (chatNode &&
       (chatNode->getType() == ChatUIDefinitions::ChatTreeNodeType::ContactsElement
       || chatNode->getType() == ChatUIDefinitions::ChatTreeNodeType::ContactsRequestElement)
      )
   {
      return static_cast<ChatContactElement*> (chatNode);
   }
   return nullptr;
}

std::shared_ptr<Chat::Data> RootItem::findMessageItem(const std::string &chatId, const std::string &messgeId)
{
   TreeItem* chatNode = findChatNode(chatId);
   if (chatNode && chatNode->isChildTypeSupported(ChatUIDefinitions::ChatTreeNodeType::MessageDataNode)) {
         for (auto child : chatNode->getChildren()) {
            auto data = static_cast<CategoryElement*>(child)->getDataObject();
            if (data && data->has_message() && data->message().id() == messgeId) {
               return data;
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

bool RootItem::insertMessageNode(DisplayableDataNode * messageNode)
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
                                       [type](TreeItem* child) {
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

void RootItem::notifyMessageChanged(std::shared_ptr<Chat::Data> message)
{
   assert(message->has_message());

   if (message) {
      std::string chatId = message->message().sender_id() == currentUser()
                       ? message->message().receiver_id()
                       : message->message().sender_id();

      TreeItem* chatNode = findChatNode(chatId);
      if (chatNode == nullptr) {
         chatId = message->message().receiver_id();
         chatNode = findChatNode(chatId);
      }

      if (chatNode && chatNode->isChildTypeSupported(ChatUIDefinitions::ChatTreeNodeType::MessageDataNode)) {
         for (auto child : chatNode->getChildren()) {
            CategoryElement * elem = static_cast<CategoryElement*>(child);

            if (elem->getDataObject()->has_message()) {
               if (message->message().id() == elem->getDataObject()->message().id()) {
                  emit itemChanged(chatNode);
               }
            } else {
               emit itemChanged(chatNode);
            }
         }
      }
   }
}

void RootItem::notifyContactChanged(std::shared_ptr<Chat::Data> contact)
{
   assert(contact->has_contact_record());

   TreeItem* chatNode = findChatNode(contact->contact_record().contact_id());
   if (chatNode && (chatNode->getType() == ChatUIDefinitions::ChatTreeNodeType::ContactsElement
                 || chatNode->getType() == ChatUIDefinitions::ChatTreeNodeType::ContactsRequestElement)
       ) {
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
      auto messageNode = dynamic_cast<TreeMessageNode*>(child);

      if (!messageNode) {
         return false;
      }

      auto message = messageNode->getMessage();
      const RootItem * root = static_cast<const RootItem*>(recursiveRoot());
      if (message
          && !ChatUtils::messageFlagRead(message->message(), Chat::Data_Message_State_READ)
          && root->currentUser() != message->message().sender_id()) {
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
