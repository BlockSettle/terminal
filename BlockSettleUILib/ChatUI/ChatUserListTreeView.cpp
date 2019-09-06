#include <QMenu>
#include <QLabel>

#include "ChatUserListTreeView.h"
#include "ChatClientUsersViewItemDelegate.h"
#include "BSMessageBox.h"
#include "EditContactDialog.h"

namespace {
   // Translation
   const QString contextMenuRemoveUser = QObject::tr("Remove from contacts");
   const QString contextMenuEditUser = QObject::tr("Edit contact");
   const QString contextMenuRemoveRequest = QObject::tr("Remove this request");
   const QString contextMenuAcceptRequest = QObject::tr("Accept friend request");
   const QString contextMenuDeclineRequest = QObject::tr("Decline friend request");

   const QString dialogRemoveContact = QObject::tr("Remove contact");
   const QString dialogRemoveCCAsContact = QObject::tr("Remove %1 as a contact?");
   const QString dialogRemoveContactAreYouSure = QObject::tr("Are you sure you wish to remove this contact?");
}

ChatUserListTreeView::ChatUserListTreeView(QWidget *parent)
   : QTreeView (parent)
{
   setContextMenuPolicy(Qt::CustomContextMenu);
   connect(this, &QAbstractItemView::customContextMenuRequested, this, &ChatUserListTreeView::onCustomContextMenu);
   setItemDelegate(new ChatClientUsersViewItemDelegate({}, this));

   connect(this, &QTreeView::clicked, this, &ChatUserListTreeView::onClicked);
   connect(this, &QTreeView::doubleClicked, this, &ChatUserListTreeView::onDoubleClicked);
}

void ChatUserListTreeView::setActiveChatLabel(QLabel *label)
{
   label_ = label;
}

void ChatUserListTreeView::editContact(const QModelIndex& index)
{
   PartyTreeItem* item = internalPartyTreeItem(index);
   if (nullptr == item) {
      return;
   }

   const Chat::ClientPartyPtr clientPartyPtr = item->data().value<Chat::ClientPartyPtr>();
   if (nullptr == clientPartyPtr) {
      return;
   }

   if (clientPartyPtr->isPrivateStandard()) {
      Chat::PartyRecipientPtr recipientPtr = clientPartyPtr->getSecondRecipient(currentUser());

      if (nullptr == recipientPtr)
      {
         return;
      }

      EditContactDialog dialog(
         QString::fromStdString(clientPartyPtr->userHash()), 
         QString::fromStdString(clientPartyPtr->displayName()), 
         recipientPtr->publicKeyTime(),
         QString::fromStdString(recipientPtr->publicKey().toHexStr()),
         parentWidget()->window());
      if (dialog.exec() == QDialog::Accepted) {
         emit setDisplayName(clientPartyPtr->id(), dialog.displayName().toStdString());
      }
   }
}

void ChatUserListTreeView::onCustomContextMenu(const QPoint & point)
{
   QModelIndex index = indexAt(point);
   emit partyClicked(index);
   if (!index.isValid()) {
      return;
   }

   PartyTreeItem* item = internalPartyTreeItem(index);
   if (nullptr == item) {
      return;
   }

   const Chat::ClientPartyPtr clientPartyPtr = item->data().value<Chat::ClientPartyPtr>();
   if (nullptr == clientPartyPtr) {
      return;
   }

   if (!clientPartyPtr->isPrivate()) { 
      return;
   }

   QMenu contextMenu;
   if (Chat::PartyState::INITIALIZED == clientPartyPtr->partyState()) {
      QAction* removeAction = contextMenu.addAction(contextMenuRemoveUser);
      removeAction->setData(index);
      connect(removeAction, &QAction::triggered, this, &ChatUserListTreeView::onRemoveFromContacts);
      contextMenu.addAction(removeAction);

      QAction* editAction = contextMenu.addAction(contextMenuEditUser);
      editAction->setData(index);
      connect(editAction, &QAction::triggered, this, &ChatUserListTreeView::onEditContact);
      contextMenu.addAction(editAction);
   }

   if (Chat::PartyState::REJECTED == clientPartyPtr->partyState()) {
      QAction* removeAction = contextMenu.addAction(contextMenuRemoveRequest);
      removeAction->setData(index);
      connect(removeAction, &QAction::triggered, this, &ChatUserListTreeView::onRemoveFromContacts);
      contextMenu.addAction(removeAction);
   }

   if (Chat::PartyState::REQUESTED == clientPartyPtr->partyState()) {
      if (clientPartyPtr->partyCreatorHash() != currentUser()) {
         // receiver of party
         QAction* acceptAction = contextMenu.addAction(contextMenuAcceptRequest);
         acceptAction->setData(index);
         connect(acceptAction, &QAction::triggered, this, &ChatUserListTreeView::onAcceptFriendRequest);
         contextMenu.addAction(acceptAction);

         QAction* declineAction = contextMenu.addAction(contextMenuDeclineRequest);
         declineAction->setData(index);
         connect(declineAction, &QAction::triggered, this, &ChatUserListTreeView::onDeclineFriendRequest);
         contextMenu.addAction(declineAction);
      }
      else {
         // creator of party
         QAction* removeAction = contextMenu.addAction(contextMenuRemoveUser);
         removeAction->setData(index);
         connect(removeAction, &QAction::triggered, this, &ChatUserListTreeView::onRemoveFromContacts);
         contextMenu.addAction(removeAction);
      }
   }

   if (contextMenu.isEmpty()) {
      return;
   }

   contextMenu.exec(viewport()->mapToGlobal(point));
   //selectionModel()->clearSelection();
}

PartyTreeItem* ChatUserListTreeView::internalPartyTreeItem(const QModelIndex& index)
{
   if (!index.isValid()) {
      return nullptr;
   }

   auto *proxyModel = static_cast<ChatPartiesSortProxyModel*>(model());
   if (!proxyModel) {
      nullptr;
   }

   return proxyModel->getInternalData(index);
}

void ChatUserListTreeView::onClicked(const QModelIndex &index)
{
   emit partyClicked(index);
   if (!index.isValid()) {
      return;
   }

   PartyTreeItem* item = internalPartyTreeItem(index);

   if (nullptr == item || UI::ElementType::Container != item->modelType()) {
      return;
   }

   if (item->data().canConvert<QString>() && item->data().toString() == ChatModelNames::ContainerTabGlobal) {
      return;
   }

   if (isExpanded(index)) {
      collapse(index);
   }
   else {
      expand(index);
   }
}

void ChatUserListTreeView::onDoubleClicked(const QModelIndex &index)
{
   emit partyClicked(index);
   PartyTreeItem* item = internalPartyTreeItem(index);

   if (nullptr == item) {
      return;
   }

   if (item->modelType() == UI::ElementType::Party) {
      editContact(index);
   }
}

void ChatUserListTreeView::updateDependUi(const QModelIndex& index)
{
   auto proxyModel = qobject_cast<const QAbstractProxyModel*>(index.model());
   QModelIndex currentIndex = proxyModel ? proxyModel->mapToSource(index) : index;
   PartyTreeItem* item = static_cast<PartyTreeItem*>(currentIndex.internalPointer());
   auto chatPartiesTreeModel = qobject_cast<const ChatPartiesTreeModel*>(currentIndex.model());

   const Chat::ClientPartyPtr clientPartyPtr = item->data().value<Chat::ClientPartyPtr>();

   if (!chatPartiesTreeModel) {
      return;
   }

   if (!clientPartyPtr) {
      return;
   }

   if (!label_) {
      return;
   }

   if (clientPartyPtr->isGlobal()) {
      label_->setText(QObject::tr("CHAT #") + QString::fromStdString(clientPartyPtr->displayName()));
   }

   if (clientPartyPtr->isPrivateStandard()) {
      QString labelPattern = QObject::tr("Contact request  #%1%2").arg(QString::fromStdString(clientPartyPtr->displayName()));
      QString stringStatus = QLatin1String("");

      if ((Chat::PartyState::UNINITIALIZED == clientPartyPtr->partyState())
         || (Chat::PartyState::REQUESTED == clientPartyPtr->partyState())) {

         if (clientPartyPtr->partyCreatorHash() == chatPartiesTreeModel->currentUser()) {
            stringStatus = QLatin1String("-OUTGOING PENDING");
         }
         else {
            stringStatus = QLatin1String("-INCOMING");
         }
         label_->setText(labelPattern.arg(stringStatus));
      }

      if (Chat::PartyState::REJECTED == clientPartyPtr->partyState()) {
         stringStatus = QLatin1String("-REJECTED");
         label_->setText(labelPattern.arg(stringStatus));
      }

      if (Chat::PartyState::INITIALIZED == clientPartyPtr->partyState()) {
         label_->setText(QObject::tr("CHAT #") + QString::fromStdString(clientPartyPtr->displayName()));
      }
   }
}

void ChatUserListTreeView::currentChanged(const QModelIndex &current, const QModelIndex &previous)
{
   QTreeView::currentChanged(current, previous);
   PartyTreeItem* item = internalPartyTreeItem(current);

   if (!item) {
      return;
   }

   if (item->modelType() == UI::ElementType::Party) {
      updateDependUi(current);
   }
}

void ChatUserListTreeView::onEditContact()
{
   QAction* action = qobject_cast<QAction*>(sender());

   if (nullptr == action) {
      return;
   }

   QModelIndex index = action->data().toModelIndex();

   editContact(index);
}

const Chat::ClientPartyPtr ChatUserListTreeView::clientPartyPtrFromAction(const QAction* action)
{
   if (nullptr == action) {
      return nullptr;
   }

   QModelIndex index = action->data().toModelIndex();

   PartyTreeItem* item = internalPartyTreeItem(index);
   if (nullptr == item) {
      return nullptr;
   }

   return item->data().value<Chat::ClientPartyPtr>();
}

const std::string& ChatUserListTreeView::currentUser() const
{
   return static_cast<ChatPartiesSortProxyModel*>(model())->currentUser();
}

void ChatUserListTreeView::onRemoveFromContacts()
{
   QAction* action = qobject_cast<QAction*>(sender());
   const Chat::ClientPartyPtr clientPartyPtr = clientPartyPtrFromAction(action);

   if (nullptr == clientPartyPtr) {
      return;
   }

   BSMessageBox confirmRemoveContact(
      BSMessageBox::question, 
      dialogRemoveContact,
      dialogRemoveCCAsContact.arg(QString::fromStdString(clientPartyPtr->displayName())),
      dialogRemoveContactAreYouSure, parentWidget()
   );

   if (confirmRemoveContact.exec() != QDialog::Accepted) {
      return;
   }

   emit removeFromContacts(clientPartyPtr->id());
}

void ChatUserListTreeView::onAcceptFriendRequest()
{
   QAction* action = qobject_cast<QAction*>(sender());
   const Chat::ClientPartyPtr clientPartyPtr = clientPartyPtrFromAction(action);

   if (nullptr == clientPartyPtr) {
      return;
   }

   emit acceptFriendRequest(clientPartyPtr->id());
}

void ChatUserListTreeView::onDeclineFriendRequest()
{
   QAction* action = qobject_cast<QAction*>(sender());
   const Chat::ClientPartyPtr clientPartyPtr = clientPartyPtrFromAction(action);

   if (nullptr == clientPartyPtr) {
      return;
   }

   emit declineFriendRequest(clientPartyPtr->id());
}
