#include "ChatClientDataModel.h"
#include <algorithm>


ChatClientDataModel::ChatClientDataModel(QObject * parent)
    : QAbstractItemModel(parent)
    , root_(std::make_shared<RootItem>())
    , newMessageMonitor_(nullptr)
{
   connect(root_.get(), &TreeItem::itemChanged, this, &ChatClientDataModel::onItemChanged);

   root_->insertItem(new CategoryItem(TreeItem::NodeType::RoomsElement));
   root_->insertItem(new CategoryItem(TreeItem::NodeType::ContactsElement));
   root_->insertItem(new CategoryItem(TreeItem::NodeType::AllUsersElement));
   //root_->insertItem(new CategoryItem(TreeItem::NodeType::SearchElement));

}

void ChatClientDataModel::clearModel()
{
   beginResetModel();
   root_->clear();
   endResetModel();
}

void ChatClientDataModel::clearSearch()
{
   TreeItem * search = root_->findCategoryNodeWith(TreeItem::NodeType::SearchElement);

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
   beginChatInsertRows(TreeItem::NodeType::RoomsElement);
   bool res = root_->insertRoomObject(data);
   endInsertRows();
   return res;
}

bool ChatClientDataModel::insertContactObject(std::shared_ptr<Chat::ContactRecordData> data, bool isOnline)
{
   beginChatInsertRows(TreeItem::NodeType::ContactsElement);
   bool res = root_->insertContactObject(data, isOnline);   
   endInsertRows();
   return res;
}

bool ChatClientDataModel::insertGeneralUserObject(std::shared_ptr<Chat::UserData> data)
{
   beginChatInsertRows(TreeItem::NodeType::AllUsersElement);
   bool res = root_->insertGeneralUserObject(data);
   endInsertRows();
   return res;
}

bool ChatClientDataModel::insertSearchUserObject(std::shared_ptr<Chat::UserData> data)
{
   beginChatInsertRows(TreeItem::NodeType::SearchElement);
   bool res = root_->insertSearchUserObject(data);
   endInsertRows();
   return res;
}

bool ChatClientDataModel::insertSearchUserList(std::vector<std::shared_ptr<Chat::UserData> > userList)
{
   if (userList.empty()) {
      return false;
   }
   TreeItem * search = root_->findCategoryNodeWith(TreeItem::NodeType::SearchElement);

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
   auto item = new TreeMessageNode(TreeItem::NodeType::RoomsElement, message);

   bool res = insertMessageNode(item);

   if (!res){
      delete  item;
   }

   return res;
}

bool ChatClientDataModel::insertContactsMessage(std::shared_ptr<Chat::MessageData> message)
{
   auto item = new TreeMessageNode(TreeItem::NodeType::ContactsElement, message);

   bool res = insertMessageNode(item);

   if (!res){
      delete  item;
   }

   return res;
}

TreeItem *ChatClientDataModel::findChatNode(const std::string &chatId)
{
   TreeItem * res =root_->findChatNode(chatId);
   return res;
}

std::vector<std::shared_ptr<Chat::ContactRecordData> > ChatClientDataModel::getAllContacts()
{
 return root_->getAllContacts();
}

bool ChatClientDataModel::removeContactNode(const std::string &contactId)
{
   TreeItem * item = root_->findCategoryNodeWith(TreeItem::NodeType::ContactsElement);

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
      case ItemAcceptTypeRole:
         return QVariant::fromValue(item->getAcceptType());
      case RoomTitleRole:
      case RoomIdRole:
         return roomData(item, role);
      case ContactIdRole:
      case ContactStatusRole:
      case ContactOnlineStatusRole:
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
      case TreeItem::NodeType::RoomsElement:
      case TreeItem::NodeType::ContactsElement:
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
   if (item->getType() == TreeItem::NodeType::RoomsElement) {
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
   if (item->getType() == TreeItem::NodeType::ContactsElement) {
      const ChatContactElement * contact_element = static_cast<const ChatContactElement*>(item);
      auto contact = contact_element->getContactData();

      if (!contact) {
         return QVariant();
      }

      switch (role) {
         case ContactIdRole:
            return contact->getContactId();
         case ContactStatusRole:
            return QVariant::fromValue(contact->getContactStatus());
         case ContactOnlineStatusRole:
            return QVariant::fromValue(contact_element->getOnlineStatus());
         default:
            return QVariant();
      }
   }
}

QVariant ChatClientDataModel::userData(const TreeItem *item, int role) const
{
   std::shared_ptr<Chat::UserData> user = nullptr;
   if (item->getType() == TreeItem::NodeType::AllUsersElement) {
      const ChatUserElement * user_element = static_cast<const ChatUserElement*>(item);
      user = user_element->getUserData();
   } else if (item->getType() == TreeItem::NodeType::SearchElement) {
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
   if (item->getAcceptType() == TreeItem::NodeType::MessageDataNode) {

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
      case TreeItem::NodeType::CategoryNode:
         if (current_flags.testFlag(Qt::ItemIsSelectable)){
            current_flags.setFlag(Qt::ItemIsSelectable, false);
         }
         break;
      case TreeItem::NodeType::SearchElement:
      case TreeItem::NodeType::RoomsElement:
      case TreeItem::NodeType::ContactsElement:
      case TreeItem::NodeType::AllUsersElement:
         if (!current_flags.testFlag(Qt::ItemIsEnabled)){
            current_flags.setFlag(Qt::ItemIsEnabled);
         }
         break;
      default:
         break;
   }


   return current_flags;
}

void ChatClientDataModel::beginChatInsertRows(const TreeItem::NodeType &type)
{
   TreeItem * item = root_->findCategoryNodeWith(type);

   if (!item) {
      return;
   }

   const QModelIndex index = createIndex(item->selfIndex(), 0, item);
   const int first = item->getChildren().empty() ? 0 : item->getChildren().back()->selfIndex();
   const int last = first + 1;

   beginInsertRows(index, first, last);
}

void ChatClientDataModel::updateNewMessagesFlag()
{
   bool flag = false;
   for (auto category : root_->getChildren()) {
      for ( auto categoryElement : category->getChildren()) {
         if (categoryElement->getAcceptType() == TreeItem::NodeType::MessageDataNode) {
            CategoryElement * elem = static_cast<CategoryElement*>(categoryElement);
            if (elem->updateNewItemsFlag()) {
               flag = true;
            }
         }
      }
   }
   newMesagesFlag_ = flag;

   if (newMessageMonitor_) {
      newMessageMonitor_->onNewMessagePresent(newMesagesFlag_);
   }
}
