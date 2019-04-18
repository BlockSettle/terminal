#include "ChatUserListTreeViewModel.h"
#include <QTreeView>

const QString contactsListDescription = QObject::tr("Contacts");
const QString publicRoomListDescription = QObject::tr("Public");

ChatUserListTreeViewModel::ChatUserListTreeViewModel(QObject* parent)
   : QAbstractItemModel(parent)
{
   rootItem_ = new ChatUserListTreeItem();
   rootItem_->addCategoryAsChild(ChatUserListTreeItem::RoomCategory);
   rootItem_->addCategoryAsChild(ChatUserListTreeItem::ContactCategory);
}

QString ChatUserListTreeViewModel::resolveCategoryDisplay(const QModelIndex &index) const
{
   switch (index.row()) {
      case ChatUserListTreeItem::RoomCategory:
         return publicRoomListDescription;

      case ChatUserListTreeItem::ContactCategory:
         return contactsListDescription;
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
      if (role == Qt::DisplayRole) {
         return resolveCategoryDisplay(index);
      }

   }

   
   return QVariant();
}

void ChatUserListTreeViewModel::setChatUserData(const ChatUserDataPtr &chatUserDataPtr)
{
   ChatUserListTreeItem *contactsItem = rootItem_->child(ChatUserListTreeItem::ContactCategory);

   int i;
   for (i = 0; i < contactsItem->childCount(); i++) {
      ChatUserListTreeItem *contactItem = contactsItem->child(i);
      if (contactItem->userData()->userId() == chatUserDataPtr->userId()) {
         const auto &index = createIndex(i, 0, contactItem);
         emit dataChanged(index, index);
      }
   }
}

void ChatUserListTreeViewModel::setChatRoomData(const Chat::RoomDataPtr &chatRoomDataPtr)
{
   ChatUserListTreeItem *roomsItem = rootItem_->child(ChatUserListTreeItem::RoomCategory);
   
   int i;
   for (i = 0; i < roomsItem->childCount(); i++) {
      ChatUserListTreeItem *roomItem = roomsItem->child(i);
      if (roomItem->roomData()->getId() == chatRoomDataPtr->getId()) {
         const auto &index = createIndex(i, 0, roomItem);
         emit dataChanged(index, index);
      }
   }
}

void ChatUserListTreeViewModel::setChatUserDataList(const ChatUserDataListPtr &chatUserDataListPtr)
{
   beginResetModel();

   ChatUserListTreeItem *contactsItem = rootItem_->child(ChatUserListTreeItem::ContactCategory);
   
   contactsItem->removeChildren();

   for (const auto &dataPtr : chatUserDataListPtr) {
      if (dataPtr->userState() != ChatUserData::State::Unknown) {
         contactsItem->addUserAsChild(dataPtr);
      }
   }

   endResetModel();
}

void ChatUserListTreeViewModel::setChatRoomDataList(const Chat::RoomDataListPtr &roomsDataList)
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

void ChatUserListTreeViewModel::selectFirstRoom()
{
   ChatUserListTreeItem *roomsItem = rootItem_->child(ChatUserListTreeItem::RoomCategory);

   if (roomsItem->childCount() > 0) {
      ChatUserListTreeItem *firstRoomItem = roomsItem->child(0);
      const auto &index = createIndex(0, 0, firstRoomItem);

      const auto treeView = qobject_cast<QTreeView *>(QObject::parent());
      treeView->selectionModel()->select(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
   }
}

void ChatUserListTreeViewModel::highlightUserItem(const QString& userId)
{
   ChatUserListTreeItem *contactsItem = rootItem_->child(ChatUserListTreeItem::ContactCategory);

   for (int i=0; i < contactsItem->childCount(); i++) {
      ChatUserListTreeItem *contactItem = contactsItem->child(i); 

      if (contactItem->userData()->userId() == userId) {
         const auto &index = createIndex(i, 0, contactItem);
         const auto treeView = qobject_cast<QTreeView *>(QObject::parent());

         if (treeView != NULL) {
            treeView->selectionModel()->select(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
         }
      }
   }
}