#include "ChatUserListTreeWidget.h"
#include "ChatUserCategoryListView.h"

#include <QHeaderView>
#include <QAbstractItemView>

const QString contactsListDescription = QObject::tr("Contacts");
const QString allUsersListDescription = QObject::tr("All users");
const QString publicRoomListDescription = QObject::tr("Public");

ChatUserListTreeWidget::ChatUserListTreeWidget(QWidget *parent) : QTreeWidget(parent)
{
   friendUsersViewModel_ = new ChatUsersViewModel(this);
   nonFriendUsersViewModel_ = new ChatUsersViewModel(this);
   publicRoomsViewModel_ = new ChatRoomsViewModel(this);

   friendUsersListView_ = new ChatUserCategoryListView(this);
   nonFriendUsersListView_ = new ChatUserCategoryListView(this);
   publicRoomsListView_ = new ChatRoomsCategoryListView(this);
   
   createCategories();
}

void ChatUserListTreeWidget::createCategories()
{
   QTreeWidgetItem *publicRoomsItem = new QTreeWidgetItem(this);
   publicRoomsItem->setText(0, publicRoomListDescription);
   addTopLevelItem(publicRoomsItem);
   setItemExpanded(publicRoomsItem, false);
   publicRoomsItem->setFlags(Qt::NoItemFlags);
   
   QTreeWidgetItem *embedPublicRoomsItem = new QTreeWidgetItem(publicRoomsItem);
   embedPublicRoomsItem->setFlags(Qt::ItemIsEnabled);
   publicRoomsListView_->setViewMode(QListView::ListMode);
   publicRoomsListView_->setModel(publicRoomsViewModel_);
   publicRoomsListView_->setObjectName(QStringLiteral("chatRoomsCategoryListView"));
   setItemWidget(embedPublicRoomsItem, 0, publicRoomsListView_);
   
   connect(publicRoomsListView_, &QAbstractItemView::clicked,
           this, &ChatUserListTreeWidget::onRoomListItemClicked);
   
   
   QTreeWidgetItem *contactsItem = new QTreeWidgetItem(this);
   contactsItem->setText(0, contactsListDescription);
   addTopLevelItem(contactsItem);
   setItemExpanded(contactsItem, false);
   contactsItem->setFlags(Qt::NoItemFlags);

   QTreeWidgetItem *embedItem = new QTreeWidgetItem(contactsItem);
   embedItem->setFlags(Qt::ItemIsEnabled);

   friendUsersListView_->setViewMode(QListView::ListMode);
   friendUsersListView_->setModel(friendUsersViewModel_);
   friendUsersListView_->setObjectName(QStringLiteral("chatUserCategoryListView"));
   setItemWidget(embedItem, 0, friendUsersListView_);

   connect(friendUsersListView_, &QAbstractItemView::clicked,
           this, &ChatUserListTreeWidget::onUserListItemClicked);

   QTreeWidgetItem *allUsers = new QTreeWidgetItem(this);
   allUsers->setText(0, allUsersListDescription);
   addTopLevelItem(allUsers);
   setItemExpanded(allUsers, false);
   allUsers->setFlags(Qt::NoItemFlags);

   QTreeWidgetItem *embedAllUsers = new QTreeWidgetItem(allUsers);
   embedAllUsers->setFlags(Qt::ItemIsEnabled);

   nonFriendUsersListView_->setViewMode(QListView::ListMode);
   nonFriendUsersListView_->setModel(nonFriendUsersViewModel_);
   nonFriendUsersListView_->setObjectName(QStringLiteral("chatUserCategoryListView"));
   setItemWidget(embedAllUsers, 0, nonFriendUsersListView_);

   connect(nonFriendUsersListView_, &QAbstractItemView::clicked,
           this, &ChatUserListTreeWidget::onUserListItemClicked);
   
   

   adjustListViewSize();
}

void ChatUserListTreeWidget::onChatUserDataListChanged(const ChatUserDataListPtr &chatUserDataList)
{
   ChatUserDataListPtr friendList;
   ChatUserDataListPtr nonFriendList;

   for (const ChatUserDataPtr &userDataPtr : chatUserDataList)
   {
      if (userDataPtr->userState() == ChatUserData::State::Unknown)
      {
         nonFriendList.push_back(userDataPtr);
      }
      else
      {
         friendList.push_back(userDataPtr);
      }
   }

   friendUsersViewModel_->onUserDataListChanged(friendList);
   nonFriendUsersViewModel_->onUserDataListChanged(nonFriendList);

   adjustListViewSize();
}

void ChatUserListTreeWidget::onChatRoomDataListChanged(const QList<std::shared_ptr<Chat::ChatRoomData> >& chatRoomDataList)
{
   QList<std::shared_ptr<Chat::ChatRoomData> > roomList;
   for (const std::shared_ptr<Chat::ChatRoomData>&dataPtr : chatRoomDataList)
   {
      roomList.push_back(dataPtr);
   }

   publicRoomsViewModel_->onRoomsDataListChanged(roomList);

   adjustListViewSize();
}

void ChatUserListTreeWidget::adjustListViewSize()
{
   int idx = topLevelItemCount();
   for (int i = 0; i < idx; i++)
   {
      QTreeWidgetItem *item = topLevelItem(i);
      QTreeWidgetItem *embedItem = item->child(0);
      if (embedItem == nullptr)
         continue;

      ChatUserCategoryListView *listWidgetUsers = qobject_cast<ChatUserCategoryListView*>(itemWidget(embedItem, 0));
      ChatRoomsCategoryListView *listWidgetRooms = qobject_cast<ChatRoomsCategoryListView*>(itemWidget(embedItem, 0));
      QListView * listWidget = nullptr;
      
      int height = -1;
      if (listWidgetUsers){
         listWidgetUsers->doItemsLayout();
         height = qMax(listWidgetUsers->contentsSize().height(), 0);
         listWidget = listWidgetUsers;
      } else {
         listWidgetRooms->doItemsLayout();
         height = qMax(listWidgetRooms->contentsSize().height(), 0);
         listWidget = listWidgetRooms;
      }
      
      listWidget->setFixedHeight(height);
      if (listWidget->model()->rowCount())
      {
         item->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
         setItemExpanded(item, true);
         item->setFlags(Qt::ItemIsEnabled);
      }
      else
      {
         item->setChildIndicatorPolicy(QTreeWidgetItem::DontShowIndicator);
         setItemExpanded(item, false);
         item->setFlags(Qt::NoItemFlags);
      }
   }
}

ChatUserCategoryListView *ChatUserListTreeWidget::listViewAt(int idx) const
{
   ChatUserCategoryListView *listWidget = nullptr;
   if (QTreeWidgetItem *item = topLevelItem(idx))
      if (QTreeWidgetItem *embedItem = item->child(0))
         listWidget = qobject_cast<ChatUserCategoryListView*>(itemWidget(embedItem, 0));
   Q_ASSERT(listWidget);

   return listWidget;
}

void ChatUserListTreeWidget::onUserListItemClicked(const QModelIndex &index)
{
   ChatUserCategoryListView *listView = qobject_cast<ChatUserCategoryListView*>(sender());

   if (!listView)
   {
      return;
   }

   if (listView == friendUsersListView_)
   {
      nonFriendUsersListView_->clearSelection();
   }
   else
   {
      friendUsersListView_->clearSelection();
   }
   publicRoomsListView_->clearSelection();

   ChatUsersViewModel *model = qobject_cast<ChatUsersViewModel*>(listView->model());
   if (!model)
   {
      return;
   }

   QString userId = model->data(index, Qt::DisplayRole).toString();
   emit userClicked(userId);
}

void ChatUserListTreeWidget::onRoomListItemClicked(const QModelIndex& index)
{
   ChatRoomsCategoryListView *listView = qobject_cast<ChatRoomsCategoryListView *>(sender());

   if (!listView)
   {
      return;
   }

   if (listView == publicRoomsListView_)
   {
      nonFriendUsersListView_->clearSelection();
      friendUsersListView_->clearSelection();
   }

   ChatRoomsViewModel *model = qobject_cast<ChatRoomsViewModel*>(listView->model());
   if (!model)
   {
      return;
   }

   QString roomId = model->data(index, ChatRoomsViewModel::IdentifierRole).toString();
   emit roomClicked(roomId);
}
