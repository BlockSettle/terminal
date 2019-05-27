#include "ChatClientDataModel.h"
#include <algorithm>

ChatClientDataModel::ChatClientDataModel(QObject * parent)
    : QAbstractItemModel(parent)
    , root_(std::make_shared<RootItem>())
    , newMessageMonitor_(nullptr)
    , modelChangesHandler_(nullptr)
{
   connect(root_.get(), &TreeItem::itemChanged, this, &ChatClientDataModel::onItemChanged);
}

void ChatClientDataModel::initTreeCategoryGroup()
{
   beginResetModel();
   root_->insertItem(new TreeCategoryGroup(ChatUIDefinitions::ChatTreeNodeType::RoomsElement, tr("Chat rooms")));
   root_->insertItem(new TreeCategoryGroup(ChatUIDefinitions::ChatTreeNodeType::ContactsElement, tr("Contacts")));
   root_->insertItem(new TreeCategoryGroup(ChatUIDefinitions::ChatTreeNodeType::AllUsersElement, tr("Users")));
   root_->insertItem(new TreeCategoryGroup(ChatUIDefinitions::ChatTreeNodeType::OTCReceivedResponsesElement, tr("Received Responses")));
   root_->insertItem(new TreeCategoryGroup(ChatUIDefinitions::ChatTreeNodeType::OTCSentResponsesElement, tr("Sent Responses")));
   //root_->insertItem(new TreeCategoryGroup(ChatUIDefinitions::ChatTreeNodeType::SearchElement, tr("Search")));
   endResetModel();
}

void ChatClientDataModel::clearModel()
{
   beginResetModel();
   //root_->clear();
   root_->deleteChildren();
   endResetModel();
}

void ChatClientDataModel::clearSearch()
{
   TreeItem * search = root_->findCategoryNodeWith(ChatUIDefinitions::ChatTreeNodeType::SearchElement);

   if (!search || search->getChildren().empty()) {
      return;
   }

   const int first = search->getChildren().front()->selfIndex();
   const int last = search->getChildren().back()->selfIndex();

   beginRemoveRows(createIndex(search->selfIndex(), 0, search), first, last);
   root_->clearSearch();
   endRemoveRows();
}

bool ChatClientDataModel::insertRoomObject(std::shared_ptr<Chat::RoomData> data)
{
   beginChatInsertRows(ChatUIDefinitions::ChatTreeNodeType::RoomsElement);
   bool res = root_->insertRoomObject(data);
   endInsertRows();
   return res;
}

bool ChatClientDataModel::insertContactObject(std::shared_ptr<Chat::ContactRecordData> data, bool isOnline)
{
   beginChatInsertRows(ChatUIDefinitions::ChatTreeNodeType::ContactsElement);
   bool res = root_->insertContactObject(data, isOnline);
   endInsertRows();
   return res;
}

bool ChatClientDataModel::insertGeneralUserObject(std::shared_ptr<Chat::UserData> data)
{
   beginChatInsertRows(ChatUIDefinitions::ChatTreeNodeType::AllUsersElement);
   bool res = root_->insertGeneralUserObject(data);
   endInsertRows();
   return res;
}

bool ChatClientDataModel::insertSearchUserObject(std::shared_ptr<Chat::UserData> data)
{
   beginChatInsertRows(ChatUIDefinitions::ChatTreeNodeType::SearchElement);
   bool res = root_->insertSearchUserObject(data);
   endInsertRows();
   return res;
}

bool ChatClientDataModel::insertSearchUserList(std::vector<std::shared_ptr<Chat::UserData> > userList)
{
   if (userList.empty()) {
      return false;
   }
   TreeItem * search = root_->findCategoryNodeWith(ChatUIDefinitions::ChatTreeNodeType::SearchElement);

   if (!search) {
      return false;
   }

   const QModelIndex index = createIndex(search->selfIndex(), 0, search);

   const int first = search->getChildren().empty()
               ? 0
               : search->getChildren().back()->selfIndex();

   const int last = first + static_cast<int>(userList.size()) - 1;

   beginInsertRows(index, first, last);
   for (auto user : userList){
      root_->insertSearchUserObject(user);
   }
   endInsertRows();

   return true;
}

bool ChatClientDataModel::insertMessageNode(TreeMessageNode * messageNode)
{
   //We use this to find target Room node were this message should be
   //It need to do before call beginInsertRows to have access to target children list
   //And make first and last indices calculation for beginInsertRows
   TreeItem * target = root_->resolveMessageTargetNode(messageNode);

   if (!target){
      return false;
   }

   const QModelIndex parentIndex = createIndex(target->selfIndex(), 0, target);

   const int first = target->getChildren().empty() ? 0 : target->getChildren().back()->selfIndex();
   const int last = first + 1;

   //Here we say that for target (parentIndex) will be inserted new children
   //That will have indices from first to last
   beginInsertRows(parentIndex, first, last);
   bool res = root_->insertMessageNode(messageNode);
   endInsertRows();

   return res;
}

bool ChatClientDataModel::insertRoomMessage(std::shared_ptr<Chat::MessageData> message)
{
   lastMessage_ = message;

   auto item = new TreeMessageNode(ChatUIDefinitions::ChatTreeNodeType::RoomsElement, message);

   bool res = insertMessageNode(item);

   if (!res){
      delete  item;
   }

   return res;
}

bool ChatClientDataModel::insertContactsMessage(std::shared_ptr<Chat::MessageData> message)
{
   lastMessage_ = message;

   auto item = new TreeMessageNode(ChatUIDefinitions::ChatTreeNodeType::ContactsElement, message);

   bool res = insertMessageNode(item);

   if (!res){
      delete  item;
   }

   return res;
}

TreeItem *ChatClientDataModel::findChatNode(const std::string &chatId)
{
   return root_->findChatNode(chatId);
}

std::vector<std::shared_ptr<Chat::ContactRecordData> > ChatClientDataModel::getAllContacts()
{
 return root_->getAllContacts();
}

bool ChatClientDataModel::removeContactNode(const std::string &contactId)
{
   TreeItem * item = root_->findCategoryNodeWith(ChatUIDefinitions::ChatTreeNodeType::ContactsElement);

   //Check if category node exists and have children that could be removed
   if (!item || item->getChildren().empty()) {
      return false;
   }

   //   If exists, we're creating index for this category node,
   //   for which children count POTENTIALLY could be changed
   const QModelIndex index = createIndex(item->selfIndex(), 0, item);

   //Trying to find contact node that contains ContactRecordData with contactId
   ChatContactElement * contactNode = root_->findContactNode(contactId);

   if (!contactNode){
      //If not found, then nothing to remove, and returning false
      return false;
   }

   //If found, we can get index of this contactNode, it can calculate self index in parent
   const int first = contactNode->selfIndex();
   //We removing only one item, so should be first==last
   const int last = first;

   beginRemoveRows(index, first, last);
   bool res = root_->removeContactNode(contactId);
   endRemoveRows();
   return res;
}

std::shared_ptr<Chat::ContactRecordData> ChatClientDataModel::findContactItem(const std::string &contactId)
{
   return root_->findContactItem(contactId);
}

ChatContactElement *ChatClientDataModel::findContactNode(const std::string &contactId)
{
   return root_->findContactNode(contactId);
}

std::shared_ptr<Chat::MessageData> ChatClientDataModel::findMessageItem(const std::string &chatId, const std::string &messgeId)
{
   return root_->findMessageItem(chatId, messgeId);
}

std::string ChatClientDataModel::currentUser() const
{
   return root_->currentUser();
}

void ChatClientDataModel::setCurrentUser(const std::string &currentUser)
{
   return root_->setCurrentUser(currentUser);
}

void ChatClientDataModel::notifyMessageChanged(std::shared_ptr<Chat::MessageData> message)
{
   return root_->notifyMessageChanged(message);
}

void ChatClientDataModel::notifyContactChanged(std::shared_ptr<Chat::ContactRecordData> contact)
{
   return root_->notifyContactChanged(contact);
}

void ChatClientDataModel::setNewMessageMonitor(NewMessageMonitor *monitor)
{
   newMessageMonitor_ = monitor;
}

QModelIndex ChatClientDataModel::index(int row, int column, const QModelIndex &parent) const
{
   if (!hasIndex(row, column, parent)){
      return QModelIndex();
   }

   TreeItem* parentItem;

   if (!parent.isValid()){
      parentItem = root_.get();
   } else {
      parentItem = static_cast<TreeItem*>(parent.internalPointer());
   }
   //std::list<TreeItem*>::iterator it = parentItem->getChildren().begin();

   TreeItem * childItem = parentItem->getChildren().at(row);

   if (childItem){
      return createIndex(row, column, childItem);
   }

   return QModelIndex();
}

QModelIndex ChatClientDataModel::parent(const QModelIndex &child) const
{
   if (!child.isValid()) {
      return QModelIndex();
   }

   TreeItem *childItem = static_cast<TreeItem*>(child.internalPointer());
   TreeItem *parentItem = childItem->getParent();

   if (root_ && parentItem == root_.get()) {
      return QModelIndex();
   }

   return createIndex(parentItem->selfIndex(), 0, parentItem);
}

int ChatClientDataModel::rowCount(const QModelIndex &parent) const
{
   TreeItem *parentItem = nullptr;
   if (parent.column() > 0) {
      return 0;
   }

   if (!parent.isValid()) {
      parentItem = root_.get();
      //return root_->notEmptyChildrenCount();
   } else {
      parentItem = static_cast<TreeItem*>(parent.internalPointer());
   }

   if (parentItem) {
      return static_cast<int>(parentItem->getChildren().size());
   }

   return 0;
}

int ChatClientDataModel::columnCount(const QModelIndex &) const
{
   return 1;
}

QVariant ChatClientDataModel::data(const QModelIndex &index, int role) const
{
   if (!index.isValid()) { //Applicable for RootNode
      return QVariant();
   }
   TreeItem *item = static_cast<TreeItem*>(index.internalPointer());
   switch (role) {
      case ItemTypeRole:
         return QVariant::fromValue(item->getType());
      case CategoryGroupDisplayName:
         return item->getDisplayName();
      case RoomTitleRole:
      case RoomIdRole:
         return roomData(item, role);
      case ContactTitleRole:
      case ContactIdRole:
      case ContactStatusRole:
      case ContactOnlineStatusRole:
      case Qt::EditRole:
         return contactData(item, role);
      case UserIdRole:
      case UserOnlineStatusRole:
         return userData(item, role);
      case ChatNewMessageRole:
         return chatNewMessageData(item, role);
      default:
         return QVariant();
   }
}

void ChatClientDataModel::onItemChanged(TreeItem *item)
{
   switch (item->getType()) {
      case ChatUIDefinitions::ChatTreeNodeType::RoomsElement:
      case ChatUIDefinitions::ChatTreeNodeType::ContactsElement:
         updateNewMessagesFlag();
      break;
      default:
         break;

   }
   QModelIndex index = createIndex(item->selfIndex(), 0, item);
   emit dataChanged(index, index);
}

QVariant ChatClientDataModel::roomData(const TreeItem *item, int role) const
{
   if (item->getType() == ChatUIDefinitions::ChatTreeNodeType::RoomsElement) {
      const ChatRoomElement * room_element = static_cast<const ChatRoomElement*>(item);
      auto room = room_element->getRoomData();

      if (!room) {
         return QVariant();
      }

      switch (role) {
         case RoomTitleRole:
            if (room->getTitle().isEmpty()) {
               return room->getId();
            }
            return room->getTitle();
         case RoomIdRole:
            return room->getId();
         default:
            return QVariant();
      }
   }
   return QVariant();
}

QVariant ChatClientDataModel::contactData(const TreeItem *item, int role) const
{
   if (item->getType() == ChatUIDefinitions::ChatTreeNodeType::ContactsElement) {
      const ChatContactElement * contact_element = static_cast<const ChatContactElement*>(item);
      auto contact = contact_element->getContactData();

      if (!contact) {
         return QVariant();
      }

      switch (role) {
         case ContactTitleRole:
            if (contact->getDisplayName().isEmpty()) {
               return contact->getContactId();
            }
            return contact->getDisplayName();
         case ContactIdRole:
            return contact->getContactId();
         case ContactStatusRole:
            return QVariant::fromValue(contact->getContactStatus());
         case ContactOnlineStatusRole:
            return QVariant::fromValue(contact_element->getOnlineStatus());
         case Qt::EditRole:
            return contact->getDisplayName();
         default:
            return QVariant();
      }
   }
   return QVariant();
}

QVariant ChatClientDataModel::userData(const TreeItem *item, int role) const
{
   std::shared_ptr<Chat::UserData> user = nullptr;
   if (item->getType() == ChatUIDefinitions::ChatTreeNodeType::AllUsersElement) {
      const ChatUserElement * user_element = static_cast<const ChatUserElement*>(item);
      user = user_element->getUserData();
   } else if (item->getType() == ChatUIDefinitions::ChatTreeNodeType::SearchElement) {
      const ChatSearchElement * search_element = static_cast<const ChatSearchElement*>(item);
      user = search_element->getUserData();
   }

   if (!user) {
      return QVariant();
   }

   switch (role) {
      case UserIdRole:
         return user->getUserId();
      case UserOnlineStatusRole:
         return QVariant::fromValue(user->getUserStatus());
      default:
         return QVariant();
   }
}

QVariant ChatClientDataModel::chatNewMessageData(const TreeItem *item, int role) const
{
   bool newMessage = false;
   if (item->isChildTypeSupported(ChatUIDefinitions::ChatTreeNodeType::MessageDataNode)) {
      switch (role) {
      case ChatNewMessageRole: {
         const CategoryElement * categoryElement = static_cast<const CategoryElement*>(item);
         if (categoryElement) {
            newMessage = categoryElement->getNewItemsFlag();
         }
      }
         break;
      default:
         break;

      }
   }
   return newMessage;
}

Qt::ItemFlags ChatClientDataModel::flags(const QModelIndex &index) const
{
   if (!index.isValid()) {
      return 0;
   }

   Qt::ItemFlags current_flags = QAbstractItemModel::flags(index);

   TreeItem *item = static_cast<TreeItem*>(index.internalPointer());

   switch (item->getType()) {
      case ChatUIDefinitions::ChatTreeNodeType::CategoryGroupNode:
         if (current_flags.testFlag(Qt::ItemIsSelectable)){
            current_flags.setFlag(Qt::ItemIsSelectable, false);
         }
         break;
      case ChatUIDefinitions::ChatTreeNodeType::ContactsElement:
         //Only contact record could be edited
         if (!current_flags.testFlag(Qt::ItemIsEditable)){
            current_flags.setFlag(Qt::ItemIsEditable);
         }
         //no break needed
      case ChatUIDefinitions::ChatTreeNodeType::RoomsElement:
      case ChatUIDefinitions::ChatTreeNodeType::SearchElement:
      case ChatUIDefinitions::ChatTreeNodeType::AllUsersElement:
         if (!current_flags.testFlag(Qt::ItemIsEnabled)){
            current_flags.setFlag(Qt::ItemIsEnabled);
         }

         break;
      default:
         break;
   }


   return current_flags;
}

void ChatClientDataModel::beginChatInsertRows(const ChatUIDefinitions::ChatTreeNodeType &type)
{
   TreeItem * item = root_->findCategoryNodeWith(type);

   if (!item) {
      return;
   }

   const QModelIndex index = createIndex(item->selfIndex(), 0, item);
   const int first = item->getChildren().empty() ? 0 : item->getChildren().back()->selfIndex();
   const int last = first;

   beginInsertRows(index, first, last);
}

void ChatClientDataModel::updateNewMessagesFlag()
{
   bool flag = false;
   CategoryElement *elem = NULL;
   std::map<QString, std::shared_ptr<Chat::MessageData>> newMessages;

   for (auto category : root_->getChildren()) {
      for ( auto categoryElement : category->getChildren()) {
         if (categoryElement->isChildTypeSupported(ChatUIDefinitions::ChatTreeNodeType::MessageDataNode)) {
            elem = static_cast<CategoryElement*>(categoryElement);
            if (elem->updateNewItemsFlag()) {
               flag = true;

               bool displayTrayNotification = true;
               auto roomItem = findChatNode(lastMessage_->receiverId().toStdString());
         
               // get display tray notification flag for room
               if (roomItem && roomItem->getType() == ChatUIDefinitions::ChatTreeNodeType::RoomsElement) {
                  auto roomElement = dynamic_cast<const ChatRoomElement*>(roomItem);
                  if (roomElement) {
                     auto roomData = roomElement->getRoomData();
                     if (roomData) {
                        displayTrayNotification = roomData->displayTrayNotification();
                     }
                  }
               }

               if (displayTrayNotification) {
                  // get contact name if exist
                  QString contactName;
                  auto contactItem = findContactItem(lastMessage_->senderId().toStdString());
                  if (contactItem && contactItem->hasDisplayName()) {
                     contactName = contactItem->getDisplayName();
                  }
                  newMessages.emplace(contactName, lastMessage_);
               }
            }
         }        
      }
   }
   newMesagesFlag_ = flag;

   if (newMessageMonitor_) {
      newMessageMonitor_->onNewMessagesPresent(newMessages);
   }
}

void ChatClientDataModel::setModelChangesHandler(ModelChangesHandler *modelChangesHandler)
{
   modelChangesHandler_ = modelChangesHandler;
}

bool ChatClientDataModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
   if (!index.isValid()) {
      return  false;
   }

   TreeItem *item = static_cast<TreeItem*>(index.internalPointer());

   switch (item->getType()) {
      case ChatUIDefinitions::ChatTreeNodeType::ContactsElement: {
         auto citem = static_cast<ChatContactElement*>(item);
         auto cdata = citem->getContactData();
         if (citem && cdata) {
            cdata->setDisplayName(value.toString());
            root_->notifyContactChanged(cdata);
            if (modelChangesHandler_) {
               modelChangesHandler_->onContactUpdatedByInput(cdata);
            }
            return true;
         }
         return false;
      }
      default:
         return QAbstractItemModel::setData(index, value, role);
   }
}

// insert channel for response that client send to OTC requests
bool ChatClientDataModel::insertOTCSentResponse(const std::string& otcId)
{
   beginChatInsertRows(ChatUIDefinitions::ChatTreeNodeType::OTCSentResponsesElement);
   bool res = root_->insertOTCSentResponseObject(otcId);
   endInsertRows();
   return res;
}

// insert channel for response client receive for own OTC
bool ChatClientDataModel::insertOTCReceivedResponse(const std::string& otcId)
{
   beginChatInsertRows(ChatUIDefinitions::ChatTreeNodeType::OTCReceivedResponsesElement);
   bool res = root_->insertOTCReceivedResponseObject(otcId);
   endInsertRows();
   return res;
}
