#ifndef CHAT_USERS_LILST_TREE_VIEW_MODEL_H
#define CHAT_USERS_LILST_TREE_VIEW_MODEL_H

#include <QAbstractItemModel>

#include "ChatUserListTreeItem.h"

class ChatUserListTreeViewModel : public QAbstractItemModel
{
   Q_OBJECT

public:
   enum class ItemType
   {
      NoneItem = 0,
      CategoryItem = 1,
      UserItem = 2,
      RoomItem = 3
   };

   enum Role
   {
      UserConnectionStatusRole = Qt::UserRole,
      ItemTypeRole,
      RoomIDRole,
      UserStateRole,
      UserNameRole,
      HaveNewMessageRole
   };

   explicit ChatUserListTreeViewModel(QObject* parent = nullptr);

public:
   QModelIndex index(int row, int column, const QModelIndex &parent) const override;
   QModelIndex parent(const QModelIndex &child) const override;

   int columnCount(const QModelIndex &parent = QModelIndex()) const override;
   int rowCount(const QModelIndex &parent = QModelIndex()) const override;

   QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
   QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

   void setChatUserDataList(const ChatUserDataListPtr &chatUserDataListPtr);
   void setChatRoomDataList(const Chat::ChatRoomDataListPtr &roomsDataList);

private:
   ChatUserListTreeItem *rootItem_;

   ChatUserListTreeItem *getItem(const QModelIndex &index) const;

   QString resolveCategoryDisplay(const QModelIndex &) const;
   ItemType resolveItemType(const QModelIndex &) const;
};

Q_DECLARE_METATYPE(ChatUserListTreeViewModel::ItemType)

#endif // CHAT_USERS_LILST_TREE_VIEW_MODEL_H
