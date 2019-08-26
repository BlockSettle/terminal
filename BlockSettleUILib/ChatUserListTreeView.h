#ifndef CHATCLIENTUSERVIEW_H
#define CHATCLIENTUSERVIEW_H

#include <QTreeView>
#include "ChatHandleInterfaces.h"
#include "ChatUsersViewItemStyle.h"
#include "ChatProtocol/ClientPartyModel.h"

class QLabel;
class QMenu;

class ChatUsersContextMenu;
class ChatUsersViewItemStyle;
class PartyTreeItem;
class ChatPartiesTreeModel;

class ChatUserListTreeView : public QTreeView
{
   Q_OBJECT

public:
   ChatUserListTreeView(QWidget * parent = nullptr);
   void addWatcher(ViewItemWatcher* watcher);
   void setActiveChatLabel(QLabel * label);
   void setHandler(ChatItemActionsHandler * handler);
   void setCurrentUserChat(const std::string &userId);
   void updateCurrentChat();
   void setChatClientServicePtr(const Chat::ClientPartyModelPtr& clientPartyModelPtr);

public slots:
   void onCustomContextMenu(const QPoint &);

protected slots:
   void currentChanged(const QModelIndex& current, const QModelIndex& previous) override;
   void dataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight, const QVector<int>& roles) override;
   void rowsInserted(const QModelIndex& parent, int start, int end) override;
   void rowsAboutToBeRemoved(const QModelIndex& parent, int start, int end) override;

private slots:
   void onClicked(const QModelIndex &);
   void onDoubleClicked(const QModelIndex &);
   void editContact(const QModelIndex& index);
   void onEditContact();
   void onRemoveFromContacts();
   void onAcceptFriendRequest();
   void onDeclineFriendRequest();

private:
   PartyTreeItem* internalPartyTreeItem(const QModelIndex& index);
   const ChatPartiesTreeModel* internalChatPartiesTreeModel(const QModelIndex& index);
   void updateDependUi(const QModelIndex& index);
   void notifyCurrentChanged(CategoryElement *element);
   void notifyMessageChanged(std::shared_ptr<Chat::Data> message);
   void notifyElementUpdated(CategoryElement *element);
   void notifyCurrentAboutToBeRemoved();

private:
   friend ChatUsersContextMenu;
   std::list<ViewItemWatcher* > watchers_;
   ChatItemActionsHandler * handler_;
   QLabel * label_;
   QMenu* contextMenu_;
   Chat::ClientPartyModelPtr clientPartyModelPtr_;
};
#endif // CHATCLIENTUSERVIEW_H
