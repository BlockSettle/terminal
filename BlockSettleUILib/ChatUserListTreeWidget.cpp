#include "ChatUserListTreeWidget.h"
#include "ChatUserCategoryListView.h"

#include <QHeaderView>
#include <QAbstractItemView>

const QString contactsListDescription = QObject::tr("Contacts");
const QString allUsersListDescription = QObject::tr("All users");

ChatUserListTreeWidget::ChatUserListTreeWidget(QWidget *parent) : QTreeWidget(parent)
{
   friendUsersViewModel_ = new ChatUsersViewModel(this);
   nonFriendUsersViewModel_ = new ChatUsersViewModel(this);

   friendUsersListView_ = new ChatUserCategoryListView(this);
   nonFriendUsersListView_ = new ChatUserCategoryListView(this);

   createCategories();
}

void ChatUserListTreeWidget::createCategories()
{
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

void ChatUserListTreeWidget::adjustListViewSize()
{
   int idx = topLevelItemCount();
   for (int i = 0; i < idx; i++)
   {
      QTreeWidgetItem *item = topLevelItem(i);
      QTreeWidgetItem *embedItem = item->child(0);
      if (embedItem == nullptr)
         continue;

      ChatUserCategoryListView *listWidget = qobject_cast<ChatUserCategoryListView*>(itemWidget(embedItem, 0));
      listWidget->doItemsLayout();
      const int height = qMax(listWidget->contentsSize().height(), 0);
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

   ChatUsersViewModel *model = qobject_cast<ChatUsersViewModel*>(listView->model());
   if (!model)
   {
      return;
   }

   QString userId = model->data(index, Qt::DisplayRole).toString();
   emit userClicked(userId);
}
