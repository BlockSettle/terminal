#include "ChatUserListTreeViewModel.h"

const QString contactsListDescription = QObject::tr("Contacts");
const QString allUsersListDescription = QObject::tr("All users");
const QString publicRoomListDescription = QObject::tr("Public");

ChatUserListTreeViewModel::ChatUserListTreeViewModel(QObject* parent)
   : QAbstractItemModel(parent)
{
   rootItem_ = new ChatUserListTreeItem();
   rootItem_->addCategoryAsChild(ChatUserListTreeItem::RoomCategory);
   rootItem_->addCategoryAsChild(ChatUserListTreeItem::ContactCategory);
   rootItem_->addCategoryAsChild(ChatUserListTreeItem::UserCategory);
}

QString ChatUserListTreeViewModel::resolveCategoryDisplay(const QModelIndex &index) const
{
   switch (index.row()) {
      case ChatUserListTreeItem::RoomCategory:
         return publicRoomListDescription;

      case ChatUserListTreeItem::ContactCategory:
         return contactsListDescription;

      case ChatUserListTreeItem::UserCategory:
         return allUsersListDescription;
   }

   return {};
}

ChatUserListTreeViewModel::ItemType ChatUserListTreeViewModel::resolveItemType(const QModelIndex &index) const
{
   const ChatUserListTreeItem *item = getItem(index);

   const auto &category = item->category();
   const auto &userDataPtr = item->userData();
   const auto &roomDataPtr = item->roomData();

   if (userDataPtr) {
      return ChatUserListTreeViewModel::ItemType::UserItem;

   }
   else if (roomDataPtr) {
      return ChatUserListTreeViewModel::ItemType::RoomItem;

   }
   else if (category != ChatUserListTreeItem::NoneCategory) {
      return ChatUserListTreeViewModel::ItemType::CategoryItem;

   }

   return ChatUserListTreeViewModel::ItemType::NoneItem;
}

QModelIndex ChatUserListTreeViewModel::index(int row, int column, const QModelIndex &parent) const
{
   if (parent.isValid() && parent.column() != 0)
      return QModelIndex();

   ChatUserListTreeItem *parentItem = getItem(parent);

   ChatUserListTreeItem *childItem = parentItem->child(row);
   if (childItem)
      return createIndex(row, column, childItem);
   else
      return QModelIndex();
}

QModelIndex ChatUserListTreeViewModel::parent(const QModelIndex &child) const
{
   if (!child.isValid())
      return QModelIndex();

   ChatUserListTreeItem *childItem = getItem(child);
   ChatUserListTreeItem *parentItem = childItem->parent();

   if (parentItem == rootItem_)
      return QModelIndex();

   return createIndex(parentItem->childNumber(), 0, parentItem);
}

int ChatUserListTreeViewModel::columnCount(const QModelIndex &/*parent*/) const
{
   return 1;
}

int ChatUserListTreeViewModel::rowCount(const QModelIndex &parent) const
{
   ChatUserListTreeItem *parentItem = getItem(parent);

   return parentItem->childCount();
}

QVariant ChatUserListTreeViewModel::headerData(int /*section*/, Qt::Orientation /*orientation*/, int /*role*/) const
{
   return QVariant();
}

QVariant ChatUserListTreeViewModel::data(const QModelIndex &index, int role) const
{
   if (!index.isValid())
   {
      return QVariant();
   }

   const ChatUserListTreeItem *item = getItem(index);
   
   const auto &type = resolveItemType(index);
   
   if (role == ItemTypeRole) {
      return QVariant::fromValue(type);
   }

   if (type == ChatUserListTreeViewModel::ItemType::UserItem) {

      const auto &userDataPtr = item->userData();

      switch (role) {
         case Qt::DisplayRole:
            return userDataPtr->userId();

         case UserConnectionStatusRole:
            return QVariant::fromValue(userDataPtr->userConnectionStatus());

         case UserStateRole:
            return QVariant::fromValue(userDataPtr->userState());

         case UserNameRole:
            return userDataPtr->userName();

         case HaveNewMessageRole:
            return userDataPtr->haveNewMessage();
      }

   }
   else if (type == ChatUserListTreeViewModel::ItemType::RoomItem) {
      const auto &roomDataPtr = item->roomData();

      switch (role) {
         case Qt::DisplayRole:
            return roomDataPtr->getTitle().isEmpty() ? roomDataPtr->getId() : roomDataPtr->getTitle();
         
         case RoomIDRole:
            return roomDataPtr->getId();

         case HaveNewMessageRole:
            return roomDataPtr->haveNewMessage();
      }

   }
   else if (type == ChatUserListTreeViewModel::ItemType::CategoryItem) {
      const auto &category = item->category();  
      
      if (role == Qt::DisplayRole) {
         return resolveCategoryDisplay(index);
      }

   }

   
   return QVariant();
}

void ChatUserListTreeViewModel::setChatUserDataList(const ChatUserDataListPtr &chatUserDataListPtr)
{
   beginResetModel();

   ChatUserListTreeItem *contactsItem = rootItem_->child(ChatUserListTreeItem::ContactCategory);
   ChatUserListTreeItem *usersItem = rootItem_->child(ChatUserListTreeItem::UserCategory);
   
   contactsItem->removeChildren();
   usersItem->removeChildren();

   for (const auto &dataPtr : chatUserDataListPtr) {
      if (dataPtr->userState() == ChatUserData::State::Unknown) {
         usersItem->addUserAsChild(dataPtr);
      }
      else {
         contactsItem->addUserAsChild(dataPtr);
      }
   }

   endResetModel();
}

void ChatUserListTreeViewModel::setChatRoomDataList(const Chat::ChatRoomDataListPtr &roomsDataList)
{
   beginResetModel();
   
   ChatUserListTreeItem *roomsItem = rootItem_->child(ChatUserListTreeItem::RoomCategory);
   roomsItem->removeChildren();

   for (const auto &dataPtr : roomsDataList) {
      roomsItem->addRoomAsChild(dataPtr);
   }

   endResetModel();
}

ChatUserListTreeItem *ChatUserListTreeViewModel::getItem(const QModelIndex &index) const
{
   if (index.isValid()) {
      ChatUserListTreeItem *item = static_cast<ChatUserListTreeItem *>(index.internalPointer());
      if (item)
         return item;
   }

   return rootItem_;
}
