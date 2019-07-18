#include "ChatClientUserView.h"
#include "ChatClientTree/TreeObjects.h"
#include "ChatClientUsersViewItemDelegate.h"
#include "ChatClientDataModel.h"
#include "BSMessageBox.h"
#include "EditContactDialog.h"
#include "ChatProtocol/ChatUtils.h"

#include <QMenu>
#include <QAbstractProxyModel>

//using ItemType = ChatUserListTreeViewModel::ItemType;
//using Role = ChatUserListTreeViewModel::Role;

class ChatUsersContextMenu : public QMenu
{
public:
   ChatUsersContextMenu(ChatItemActionsHandler * handler, ChatClientUserView * parent = nullptr) :
      QMenu(parent),
      handler_(handler),
      view_(parent)
   {
      connect(this, &QMenu::aboutToHide, this, &ChatUsersContextMenu::clearMenu);
   }

   ~ChatUsersContextMenu()
   {
   }

   QAction* execMenu(const QPoint & point)
   {
      auto currentIndex = view_->indexAt(point);
      auto proxyModel = qobject_cast<const QAbstractProxyModel*>(currentIndex.model());
      currentIndex_ = proxyModel ? proxyModel->mapToSource(currentIndex) : currentIndex;

      clear();
      currentContact_.reset();
      //ItemType type = static_cast<ItemType>(currentIndex_.data(Role::ItemTypeRole).toInt());
      TreeItem * item = static_cast<TreeItem*>(currentIndex_.internalPointer());

      if (item && (item->getType() == ChatUIDefinitions::ChatTreeNodeType::ContactsElement
                || item->getType() == ChatUIDefinitions::ChatTreeNodeType::ContactsRequestElement)) {
         auto citem = static_cast<ChatContactElement*>(item);
         currentContact_ = citem->getDataObject();
         prepareContactMenu();
         return exec(view_->viewport()->mapToGlobal(point));
      }

      view_->selectionModel()->clearSelection();
      return nullptr;

   }

private slots:

   void clearMenu() {
      view_->selectionModel()->clearSelection();
   }

   void onAddToContacts(bool)
   {
      if (!handler_) {
         return;
      }
      //handler_->onActionAddToContacts(currentContact_);
   }

   void onRemoveFromContacts(bool)
   {
      if (!handler_) {
         return;
      }

      auto name = currentContact_->contact_record().display_name();
      if (name.empty()) {
         name = currentContact_->contact_record().contact_id();
      }

      BSMessageBox confirmRemoveContact(BSMessageBox::question, tr("Remove contact")
         , tr("Remove %1 as a contact?").arg(QString::fromStdString(name))
         , tr("Are you sure you wish to remove this contact?"), view_->parentWidget());

      if (confirmRemoveContact.exec() != QDialog::Accepted) {
         return;
      }

      handler_->onActionRemoveFromContacts(currentContact_);
   }

   void onAcceptFriendRequest(bool)
   {
      if (!handler_) {
         return;
      }
      handler_->onActionAcceptContactRequest(currentContact_);
   }

   void onDeclineFriendRequest(bool)
   {
      if (!handler_) {
         return;
      }
      handler_->onActionRejectContactRequest(currentContact_);
   }

   void onEditContact() {
      view_->editContact(currentContact_);
   }

   void prepareContactMenu()
   {
      if (!currentContact_) {
         return;
      }

      switch (currentContact_->contact_record().status()) {
//         case Chat::ContactStatus::
//            addAction(tr("Add friend"), this, &ChatUsersContextMenu::onAddToContacts);
//            break;
         case Chat::ContactStatus::CONTACT_STATUS_ACCEPTED:
            addAction(tr("Remove from contacts"), this, &ChatUsersContextMenu::onRemoveFromContacts);
            addAction(tr("Edit contact"), this, &ChatUsersContextMenu::onEditContact);
            break;
         case Chat::ContactStatus::CONTACT_STATUS_INCOMING:
            addAction(tr("Accept friend request"), this, &ChatUsersContextMenu::onAcceptFriendRequest);
            addAction(tr("Decline friend request"), this, &ChatUsersContextMenu::onDeclineFriendRequest);
            break;
         case Chat::ContactStatus::CONTACT_STATUS_OUTGOING:
         case Chat::ContactStatus::CONTACT_STATUS_REJECTED:
            //addAction(tr("This request is not accepted"));
            addAction(tr("Remove this request"), this, &ChatUsersContextMenu::onRemoveFromContacts);
            break;
         default:
            break;

      }
   }

   void prepareRoomMenu()
   {

   }

private:
   ChatItemActionsHandler* handler_;
   ChatClientUserView * view_;
   QModelIndex currentIndex_;
   std::shared_ptr<Chat::Data> currentContact_;
};


ChatClientUserView::ChatClientUserView(QWidget *parent)
   : QTreeView (parent),
     handler_(nullptr),
     contextMenu_(nullptr)
{
   setContextMenuPolicy(Qt::CustomContextMenu);
   connect(this, &QAbstractItemView::customContextMenuRequested, this, &ChatClientUserView::onCustomContextMenu);
   setItemDelegate(new ChatClientUsersViewItemDelegate(this));

   // expand/collapse categories only on single click
   setExpandsOnDoubleClick(false);
   connect(this, &QTreeView::clicked, this, &ChatClientUserView::onClicked);
   connect(this, &QTreeView::doubleClicked, this, &ChatClientUserView::onDoubleClicked);
}

void ChatClientUserView::addWatcher(ViewItemWatcher * watcher)
{
   watchers_.push_back(watcher);
}

void ChatClientUserView::setActiveChatLabel(QLabel *label)
{
   label_ = label;
}

void ChatClientUserView::setCurrentUserChat(const std::string &userId)
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

void ChatClientUserView::updateCurrentChat()
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
            updateDependUI(element);
            notifyCurrentChanged(element);
         }
         break;
         default:
            break;
      }
   }
}

void ChatClientUserView::editContact(std::shared_ptr<Chat::Data> crecord)
{
   if (handler_) {
      auto contactRecord = crecord->mutable_contact_record();
      QString contactId = QString::fromStdString(contactRecord->contact_id());
      QString displayName = QString::fromStdString(contactRecord->display_name());
      QDateTime timestamp = QDateTime::fromMSecsSinceEpoch(contactRecord->public_key_timestamp());
      QByteArray pubKey = QByteArray::fromStdString(contactRecord->public_key());
      QString idKey = QString::fromLatin1(pubKey.toHex());
      if (contactId == displayName) {
         displayName.clear();
      }
      EditContactDialog dialog(contactId, displayName, timestamp, idKey, parentWidget()->window());
      if (dialog.exec() == QDialog::Accepted) {
         contactRecord->set_display_name(dialog.displayName().toStdString());
         handler_->onActionEditContactRequest(crecord);
      }
   }
}

void ChatClientUserView::onCustomContextMenu(const QPoint & point)
{
   if (!contextMenu_) {
      if (handler_) {
         contextMenu_ = new ChatUsersContextMenu(handler_, this);
      }
   }
   if (contextMenu_) {
      contextMenu_->execMenu(point);
   }
}

void ChatClientUserView::onClicked(const QModelIndex &index)
{
   if (index.isValid()) {
      const auto nodeType = qvariant_cast<ChatUIDefinitions::ChatTreeNodeType>(index.data(ChatClientDataModel::Role::ItemTypeRole));

      if (nodeType == ChatUIDefinitions::ChatTreeNodeType::CategoryGroupNode) {
         if (isExpanded(index)) {
            collapse(index);
         }
         else {
            expand(index);
         }
      }
   }
}

void ChatClientUserView::onDoubleClicked(const QModelIndex &index)
{
   if (index.isValid()) {
      auto proxyModel = qobject_cast<const QAbstractProxyModel*>(index.model());
      QModelIndex i = proxyModel ? proxyModel->mapToSource(index) : index;
      TreeItem *item = static_cast<TreeItem*>(i.internalPointer());
      if (item && item->getType() == ChatUIDefinitions::ChatTreeNodeType::ContactsElement) {
         editContact(static_cast<ChatContactElement*>(item)->getDataObject());
      }
   }
}

void ChatClientUserView::updateDependUI(CategoryElement *element)
{
   auto data = static_cast<CategoryElement*>(element)->getDataObject();
   switch (element->getType()) {
      case ChatUIDefinitions::ChatTreeNodeType::RoomsElement:{
         if (label_) {
            label_->setText(QObject::tr("CHAT #") + QString::fromStdString(data->room().id()));
         }
      } break;
      case ChatUIDefinitions::ChatTreeNodeType::ContactsElement:{
         if (label_) {
            label_->setText(QObject::tr("CHAT #") + QString::fromStdString(data->contact_record().contact_id()));
         }
      } break;
      case ChatUIDefinitions::ChatTreeNodeType::ContactsRequestElement:{
         if (label_) {
            QString labelPattern = QObject::tr("Contact request  #%1%2")
                                           .arg(QString::fromStdString(data->contact_record().contact_id()));
            QString stringStatus = QLatin1String("");

            switch (data->contact_record().status()) {
               case Chat::ContactStatus::CONTACT_STATUS_INCOMING:
                  stringStatus = QLatin1String("-INCOMING");
                  break;
               case Chat::ContactStatus::CONTACT_STATUS_OUTGOING:
                  stringStatus = QLatin1String("-OUTGOING SEND");
                  break;
               case Chat::ContactStatus::CONTACT_STATUS_OUTGOING_PENDING:
                  stringStatus = QLatin1String("-OUTGOING PENDING");
                  break;
               case Chat::ContactStatus::CONTACT_STATUS_REJECTED:
                  stringStatus = QLatin1String("-REJECTED");
                  break;
               default:
                  stringStatus =
                        QString::fromStdString(Chat::ContactStatus_Name(data->contact_record().status()))
                        .prepend (QLatin1Char('-'));
                  break;
            }

            label_->setText(labelPattern.arg(stringStatus));
         }
      } break;
      case ChatUIDefinitions::ChatTreeNodeType::AllUsersElement:{
         if (label_) {
            label_->setText(QObject::tr("CHAT #") + QString::fromStdString(data->user().user_id()));
         }
      } break;
      //XXXOTC
      // case ChatUIDefinitions::ChatTreeNodeType::OTCReceivedResponsesElement:
      // case ChatUIDefinitions::ChatTreeNodeType::OTCSentResponsesElement:{
      //    if (label_) {
      //       label_->setText(QObject::tr("Trading with ..."));
      //    }
      // } break;
      default:
         break;

   }
}

void ChatClientUserView::notifyCurrentChanged(CategoryElement *element)
{
   for (auto watcher : watchers_) {
      watcher->onElementSelected(element);
   }

}

void ChatClientUserView::notifyMessageChanged(std::shared_ptr<Chat::Data> message)
{
   for (auto watcher : watchers_) {
      watcher->onMessageChanged(message);
   }
}

void ChatClientUserView::notifyElementUpdated(CategoryElement *element)
{
   for (auto watcher : watchers_) {
      watcher->onElementUpdated(element);
   }
}

void ChatClientUserView::notifyCurrentAboutToBeRemoved()
{
   for (auto watcher : watchers_) {
      watcher->onCurrentElementAboutToBeRemoved();
   }
}

void ChatClientUserView::setHandler(ChatItemActionsHandler * handler)
{
   handler_ = handler;
}

void ChatClientUserView::currentChanged(const QModelIndex &current, const QModelIndex &previous)
{
   QTreeView::currentChanged(current, previous);
   auto proxyModel = qobject_cast<const QAbstractProxyModel*>(current.model());
   QModelIndex index = proxyModel ? proxyModel->mapToSource(current) : current;
   TreeItem* item = static_cast<TreeItem*>(index.internalPointer());
   if (!watchers_.empty() && item) {
      switch (item->getType()) {
         case ChatUIDefinitions::ChatTreeNodeType::RoomsElement:
         case ChatUIDefinitions::ChatTreeNodeType::ContactsElement:
         case ChatUIDefinitions::ChatTreeNodeType::ContactsRequestElement:
         case ChatUIDefinitions::ChatTreeNodeType::AllUsersElement:{
            auto element = static_cast<CategoryElement*>(item);
            updateDependUI(element);
            notifyCurrentChanged(element);
         }
         break;
         default:
            break;

      }
   }
}

void ChatClientUserView::dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles)
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
            updateDependUI(node);
            notifyElementUpdated(node);
            break;
         }
         default:
            break;
      }
   }
}

void ChatClientUserView::rowsInserted(const QModelIndex &parent, int start, int end)
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

void ChatClientUserView::rowsAboutToBeRemoved(const QModelIndex &parent, int start, int end)
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
