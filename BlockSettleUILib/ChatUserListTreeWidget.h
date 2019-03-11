#ifndef CHATUSERLISTTREEWIDGET_H
#define CHATUSERLISTTREEWIDGET_H

#include <QTreeWidget>

#include "ChatUserData.h"
#include "ChatUsersViewModel.h"
#include "ChatUserCategoryListView.h"
#include "ChatRoomsViewModel.h"
#include "ChatRoomsCategoryListView.h"
#include "ChatProtocol/DataObjects.h"

class ChatUserListTreeWidget : public QTreeWidget
{
   Q_OBJECT
public:
   explicit ChatUserListTreeWidget(QWidget *parent = nullptr);

signals:
   void userClicked(const QString &userId);
   void roomClicked(const QString &roomId);

public slots:
   void onChatUserDataListChanged(const ChatUserDataListPtr &chatUserDataList);
   void onChatRoomDataListChanged(const QList<std::shared_ptr<Chat::ChatRoomData>> &chatRoomDataList);

private slots:
   void onUserListItemClicked(const QModelIndex &index);
   void onRoomListItemClicked(const QModelIndex &index);

private:
   void createCategories();
   void adjustListViewSize();
   ChatUserCategoryListView *listViewAt(int idx) const;

   ChatUsersViewModel *_friendUsersViewModel;
   ChatUsersViewModel *_nonFriendUsersViewModel;
   ChatUserCategoryListView *_friendUsersListView;
   ChatUserCategoryListView *_nonFriendUsersListView;
   ChatRoomsViewModel *_publicRoomsViewModel;
   ChatRoomsCategoryListView* _publicRoomsListView;
};

#endif // CHATUSERLISTTREEWIDGET_H
