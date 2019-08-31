#ifndef CHATCLIENTUSERVIEW_H
#define CHATCLIENTUSERVIEW_H

#include <QTreeView>
#include "ChatUsersViewItemStyle.h"
#include "ChatProtocol/ChatClientService.h"

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
   void setActiveChatLabel(QLabel * label);
   void setChatClientServicePtr(const Chat::ChatClientServicePtr& chatClientServicePtr);

public slots:
   void onCustomContextMenu(const QPoint &);

protected slots:
   void currentChanged(const QModelIndex& current, const QModelIndex& previous) override;
   void rowsInserted(const QModelIndex& parent, int start, int end) override;

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
   const Chat::ClientPartyPtr clientPartyPtrFromAction(const QAction* action);

private:
   friend ChatUsersContextMenu;
   QLabel * label_;
   QMenu* contextMenu_;
   Chat::ChatClientServicePtr chatClientServicePtr_;
};

#endif // CHATCLIENTUSERVIEW_H
