#include <QMenu>
#include <QAbstractProxyModel>
#include <QLabel>

#include "ChatUserListTreeView.h"
#include "ChatClientTree/TreeObjects.h"
#include "ChatClientUsersViewItemDelegate.h"
#include "ChatClientDataModel.h"
#include "BSMessageBox.h"
#include "EditContactDialog.h"
#include "ChatProtocol/ChatUtils.h"

//using ItemType = ChatUserListTreeViewModel::ItemType;
//using Role = ChatUserListTreeViewModel::Role;

ChatUserListTreeView::ChatUserListTreeView(QWidget *parent)
   : QTreeView (parent),
     handler_(nullptr),
     contextMenu_(nullptr)
{
   setContextMenuPolicy(Qt::CustomContextMenu);
   connect(this, &QAbstractItemView::customContextMenuRequested, this, &ChatUserListTreeView::onCustomContextMenu);
   setItemDelegate(new ChatClientUsersViewItemDelegate({}, this));

   // expand/collapse categories only on single click
   setExpandsOnDoubleClick(false);
   connect(this, &QTreeView::clicked, this, &ChatUserListTreeView::onClicked);
   connect(this, &QTreeView::doubleClicked, this, &ChatUserListTreeView::onDoubleClicked);
}

// out?
void ChatUserListTreeView::addWatcher(ViewItemWatcher * watcher)
{
   watchers_.push_back(watcher);
}

void ChatUserListTreeView::setActiveChatLabel(QLabel *label)
{
   label_ = label;
}

// out?
void ChatUserListTreeView::setCurrentUserChat(const std::string &userId)
{
   // find all indexes
   QModelIndexList indexes = model()->match(model()->index(0,0),
                                            Qt::DisplayRole,
                                            QLatin1String("*"),
                                            -1,
                                            Qt::MatchWildcard|Qt::MatchRecursive);

   // set required chat
   for (auto index : indexes) {
      auto type = index.data(ChatClientDataModel::Role::ItemTypeRole).value<ChatUIDefinitions::ChatTreeNodeType>();
      if (userId == " " && type == ChatUIDefinitions::ChatTreeNodeType::RoomsElement) {
         if (index.data(ChatClientDataModel::Role::RoomIdRole).toString() == QString::fromLatin1(ChatUtils::GlobalRoomKey)) {
            setCurrentIndex(index);
            break;
         }
      }
      if (type == ChatUIDefinitions::ChatTreeNodeType::ContactsElement
          || type == ChatUIDefinitions::ChatTreeNodeType::ContactsRequestElement) {
         if (index.data(ChatClientDataModel::Role::ContactIdRole).toString().toStdString() == userId) {
            setCurrentIndex(index);
            break;
         }
      }
   }
}

// out?
void ChatUserListTreeView::updateCurrentChat()
{
   auto proxyModel = qobject_cast<const QAbstractProxyModel*>(currentIndex().model());
   QModelIndex index = proxyModel ? proxyModel->mapToSource(currentIndex()) : currentIndex();
   TreeItem *item = static_cast<TreeItem*>(index.internalPointer());
   if (!watchers_.empty() && item) {
      switch (item->getType()) {
         case ChatUIDefinitions::ChatTreeNodeType::RoomsElement:
      case ChatUIDefinitions::ChatTreeNodeType::ContactsElement:
      case ChatUIDefinitions::ChatTreeNodeType::ContactsRequestElement: {
            auto element = static_cast<CategoryElement*>(item);
            //updateDependUI(element);
            notifyCurrentChanged(element);
         }
         break;
         default:
            break;
      }
   }
}

void ChatUserListTreeView::editContact(const QModelIndex& index)
{
   PartyTreeItem* item = internalPartyTreeItem(index);
   if (nullptr == item)
   {
      return;
   }

   const Chat::ClientPartyPtr clientPartyPtr = item->data().value<Chat::ClientPartyPtr>();
   if (nullptr == clientPartyPtr)
   {
      return;
   }

   auto chatPartiesTreeModel = internalChatPartiesTreeModel(index);
   if (nullptr == chatPartiesTreeModel)
   {
      return;
   }

   if (clientPartyPtr->isPrivateStandard())
   {
      Chat::PartyRecipientPtr recipient = clientPartyPtr->getSecondRecipient(chatPartiesTreeModel->currentUser());
      EditContactDialog dialog(
         QString::fromStdString(clientPartyPtr->userHash()), 
         QString::fromStdString(clientPartyPtr->displayName()), 
         recipient->publicKeyTime(), 
         QString::fromStdString(recipient->publicKey().toHexStr()),
         parentWidget()->window());
      if (dialog.exec() == QDialog::Accepted)
      {
         clientPartyPtr->setDisplayName(dialog.displayName().toStdString());
      }
   }
}

void ChatUserListTreeView::onCustomContextMenu(const QPoint & point)
{
   if (nullptr == contextMenu_)
   {
      contextMenu_ = new QMenu(this);
   }

   contextMenu_->clear();

   QModelIndex index = indexAt(point);
   if (!index.isValid())
   {
      return;
   }

   PartyTreeItem* item = internalPartyTreeItem(index);
   if (nullptr == item)
   {
      return;
   }

   const Chat::ClientPartyPtr clientPartyPtr = item->data().value<Chat::ClientPartyPtr>();
   if (nullptr == clientPartyPtr)
   {
      return;
   }

   if (!clientPartyPtr->isPrivate())
   { 
      return;
   }

   auto chatPartiesTreeModel = internalChatPartiesTreeModel(index);

   if (Chat::PartyState::INITIALIZED == clientPartyPtr->partyState())
   {
      QAction* removeAction = new QAction(tr("Remove from contacts"), this);
      removeAction->setData(index);
      connect(removeAction, &QAction::triggered, this, &ChatUserListTreeView::onRemoveFromContacts);
      contextMenu_->addAction(removeAction);

      QAction* editAction = new QAction(tr("Edit contact"), this);
      editAction->setData(index);
      connect(editAction, &QAction::triggered, this, &ChatUserListTreeView::onEditContact);
      contextMenu_->addAction(editAction);
   }

   if (Chat::PartyState::REJECTED == clientPartyPtr->partyState())
   {
      QAction* removeAction = new QAction(tr("Remove this request"), this);
      removeAction->setData(index);
      connect(removeAction, &QAction::triggered, this, &ChatUserListTreeView::onRemoveFromContacts);
      contextMenu_->addAction(removeAction);
   }

   if (Chat::PartyState::REQUESTED == clientPartyPtr->partyState())
   {
      if (clientPartyPtr->partyCreatorHash() != chatPartiesTreeModel->currentUser())
      {
         // receiver of party
         QAction* acceptAction = new QAction(tr("Accept friend request"), this);
         acceptAction->setData(index);
         connect(acceptAction, &QAction::triggered, this, &ChatUserListTreeView::onAcceptFriendRequest);
         contextMenu_->addAction(acceptAction);

         QAction* declineAction = new QAction(tr("Decline friend request"), this);
         declineAction->setData(index);
         connect(declineAction, &QAction::triggered, this, &ChatUserListTreeView::onDeclineFriendRequest);
         contextMenu_->addAction(declineAction);
      }
      else
      {
         // creator of party
         QAction* removeAction = new QAction(tr("Remove from contacts"), this);
         removeAction->setData(index);
         connect(removeAction, &QAction::triggered, this, &ChatUserListTreeView::onRemoveFromContacts);
         contextMenu_->addAction(removeAction);
      }
   }

   if (contextMenu_->isEmpty())
   {
      return;
   }

   contextMenu_->exec(viewport()->mapToGlobal(point));
   selectionModel()->clearSelection();
}

const ChatPartiesTreeModel* ChatUserListTreeView::internalChatPartiesTreeModel(const QModelIndex& index)
{
   auto proxyModel = qobject_cast<const QAbstractProxyModel*>(index.model());
   QModelIndex currentIndex = proxyModel ? proxyModel->mapToSource(index) : index;
   const ChatPartiesTreeModel* chatPartiesTreeModel = qobject_cast<const ChatPartiesTreeModel*>(currentIndex.model());

   return chatPartiesTreeModel;
}

PartyTreeItem* ChatUserListTreeView::internalPartyTreeItem(const QModelIndex& index)
{
   if (index.isValid())
   {
      auto proxyModel = qobject_cast<const QAbstractProxyModel*>(index.model());
      QModelIndex currentIndex = proxyModel ? proxyModel->mapToSource(index) : index;
      PartyTreeItem* item = static_cast<PartyTreeItem*>(currentIndex.internalPointer());

      return item;
   }

   return nullptr;
}

void ChatUserListTreeView::onClicked(const QModelIndex &index)
{
   if (index.isValid())
   {
      PartyTreeItem* item = internalPartyTreeItem(index);

      if (nullptr == item)
      {
         return;
      }

      if (UI::ElementType::Container == item->modelType())
      {
         if (isExpanded(index))
         {
            collapse(index);
         }
         else
         {
            expand(index);
         }
      }
   }
}

void ChatUserListTreeView::onDoubleClicked(const QModelIndex &index)
{
   PartyTreeItem* item = internalPartyTreeItem(index);

   if (nullptr == item)
   {
      return;
   }

   if (item->modelType() == UI::ElementType::Party)
   {
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

   if (!chatPartiesTreeModel)
   {
      return;
   }

   if (!clientPartyPtr)
   {
      return;
   }

   if (!label_)
   {
      return;
   }

   if (clientPartyPtr->isGlobal())
   {
      label_->setText(QObject::tr("CHAT #") + QString::fromStdString(clientPartyPtr->displayName()));
   }

   if (clientPartyPtr->isPrivateStandard())
   {
      QString labelPattern = QObject::tr("Contact request  #%1%2").arg(QString::fromStdString(clientPartyPtr->displayName()));
      QString stringStatus = QLatin1String("");

      if ((Chat::PartyState::UNINITIALIZED == clientPartyPtr->partyState())
         || (Chat::PartyState::REQUESTED == clientPartyPtr->partyState()))
      {
         if (clientPartyPtr->partyCreatorHash() == chatPartiesTreeModel->currentUser())
         {
            stringStatus = QLatin1String("-OUTGOING PENDING");
         }
         else
         {
            stringStatus = QLatin1String("-INCOMING");
         }
         label_->setText(labelPattern.arg(stringStatus));
      }

      if (Chat::PartyState::REJECTED == clientPartyPtr->partyState())
      {
         stringStatus = QLatin1String("-REJECTED");
         label_->setText(labelPattern.arg(stringStatus));
      }

      if (Chat::PartyState::INITIALIZED == clientPartyPtr->partyState())
      {
         label_->setText(QObject::tr("CHAT #") + QString::fromStdString(clientPartyPtr->displayName()));
      }
   }
}

// out?
void ChatUserListTreeView::notifyCurrentChanged(CategoryElement *element)
{
   for (auto watcher : watchers_) {
      watcher->onElementSelected(element);
   }

}

// out?
void ChatUserListTreeView::notifyMessageChanged(std::shared_ptr<Chat::Data> message)
{
   for (auto watcher : watchers_) {
      watcher->onMessageChanged(message);
   }
}

// out?
void ChatUserListTreeView::notifyElementUpdated(CategoryElement *element)
{
   for (auto watcher : watchers_) {
      watcher->onElementUpdated(element);
   }
}

// out?
void ChatUserListTreeView::notifyCurrentAboutToBeRemoved()
{
   for (auto watcher : watchers_) {
      watcher->onCurrentElementAboutToBeRemoved();
   }
}

// out?
void ChatUserListTreeView::setHandler(ChatItemActionsHandler * handler)
{
   handler_ = handler;
}

void ChatUserListTreeView::currentChanged(const QModelIndex &current, const QModelIndex &previous)
{
   QTreeView::currentChanged(current, previous);
   auto proxyModel = qobject_cast<const QAbstractProxyModel*>(current.model());
   QModelIndex index = proxyModel ? proxyModel->mapToSource(current) : current;
   PartyTreeItem* item = static_cast<PartyTreeItem*>(index.internalPointer());

   if (!item)
   {
      return;
   }

   if (item->modelType() == UI::ElementType::Party)
   {
      updateDependUi(current);
   }
}

void ChatUserListTreeView::dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles)
{
   QTreeView::dataChanged(topLeft, bottomRight, roles);
   if (topLeft == bottomRight) {
      auto proxyModel = qobject_cast<const QAbstractProxyModel*>(topLeft.model());
      QModelIndex index = proxyModel ? proxyModel->mapToSource(topLeft) : topLeft;
      TreeItem* item = static_cast<TreeItem*>(index.internalPointer());
      switch (item->getType()) {
         case ChatUIDefinitions::ChatTreeNodeType::MessageDataNode: {
            auto mnode = static_cast<TreeMessageNode*>(item);
            notifyMessageChanged(mnode->getMessage());
            break;
         }
         case ChatUIDefinitions::ChatTreeNodeType::RoomsElement:
         case ChatUIDefinitions::ChatTreeNodeType::ContactsElement:
         case ChatUIDefinitions::ChatTreeNodeType::ContactsRequestElement:
         {
            auto node = static_cast<CategoryElement*>(item);
            //updateDependUI(node);
            notifyElementUpdated(node);
            break;
         }
         default:
            break;
      }
   }
}

void ChatUserListTreeView::rowsInserted(const QModelIndex &parent, int start, int end)
{
   // ChatUIDefinitions::ChatTreeNodeType type = parent.data(ChatClientDataModel::Role::ItemTypeRole).value<ChatUIDefinitions::ChatTreeNodeType>();
   // ChatUIDefinitions::ChatTreeNodeType supportType = parent.data(ChatClientDataModel::Role::ItemAcceptTypeRole).value<ChatUIDefinitions::ChatTreeNodeType>();

   // if (type == ChatUIDefinitions::ChatTreeNodeType::CategoryGroupNode)
   //    if (supportType == ChatUIDefinitions::ChatTreeNodeType::SearchElement) {
   //       if (!isExpanded(parent)) {
   //          expand(parent);
   //       }
   // }

   QTreeView::rowsInserted(parent, start, end);
}

void ChatUserListTreeView::rowsAboutToBeRemoved(const QModelIndex &parent, int start, int end)
{
   bool callDefaultSelection = false;

   for (int i = start; i <= end; i++) {
      if (model()->index(i, 0, parent) == currentIndex()) {
         callDefaultSelection = true;
         break;
      }
   }

   //I'm using callDefaultSelection flag in case if
   //default element that will be selected will be in start to end range
   if (callDefaultSelection && handler_) {
      notifyCurrentAboutToBeRemoved();
   }
}

void ChatUserListTreeView::setChatClientServicePtr(const Chat::ChatClientServicePtr& chatClientServicePtr)
{
   chatClientServicePtr_ = chatClientServicePtr;
}

void ChatUserListTreeView::onEditContact()
{
   QAction* action = qobject_cast<QAction*>(sender());

   if (nullptr == action)
   {
      return;
   }

   QModelIndex index = action->data().toModelIndex();

   editContact(index);
}

const Chat::ClientPartyPtr ChatUserListTreeView::clientPartyPtrFromAction(const QAction* action)
{
   if (nullptr == action)
   {
      return nullptr;
   }

   QModelIndex index = action->data().toModelIndex();

   PartyTreeItem* item = internalPartyTreeItem(index);
   if (nullptr == item)
   {
      return nullptr;
   }

   return item->data().value<Chat::ClientPartyPtr>();
}

void ChatUserListTreeView::onRemoveFromContacts()
{
   QAction* action = qobject_cast<QAction*>(sender());
   const Chat::ClientPartyPtr clientPartyPtr = clientPartyPtrFromAction(action);

   if (nullptr == clientPartyPtr)
   {
      return;
   }

   BSMessageBox confirmRemoveContact(
      BSMessageBox::question, 
      tr("Remove contact"),
      tr("Remove %1 as a contact?").arg(QString::fromStdString(clientPartyPtr->displayName())),
      tr("Are you sure you wish to remove this contact?"), parentWidget()
   );

   if (confirmRemoveContact.exec() != QDialog::Accepted)
   {
      return;
   }

   chatClientServicePtr_->DeletePrivateParty(clientPartyPtr->id());
}

void ChatUserListTreeView::onAcceptFriendRequest()
{
   QAction* action = qobject_cast<QAction*>(sender());
   const Chat::ClientPartyPtr clientPartyPtr = clientPartyPtrFromAction(action);

   if (nullptr == clientPartyPtr)
   {
      return;
   }

   chatClientServicePtr_->AcceptPrivateParty(clientPartyPtr->id());
}

void ChatUserListTreeView::onDeclineFriendRequest()
{
   QAction* action = qobject_cast<QAction*>(sender());
   const Chat::ClientPartyPtr clientPartyPtr = clientPartyPtrFromAction(action);

   if (nullptr == clientPartyPtr)
   {
      return;
   }

   chatClientServicePtr_->RejectPrivateParty(clientPartyPtr->id());

}