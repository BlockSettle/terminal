#include "ChatClientUserView.h"
#include "ChatClientTree/TreeObjects.h"
#include "ChatClientUsersViewItemDelegate.h"
#include "ChatClientDataModel.h"

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
      qDebug() << __func__;
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

      if (item && item->getType() == ChatUIDefinitions::ChatTreeNodeType::ContactsElement) {
         auto citem = static_cast<ChatContactElement*>(item);
         currentContact_ = citem->getContactData();
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
      qDebug() << __func__;
      if (!handler_){
         return;
      }
      //handler_->onActionAddToContacts(currentContact_);
   }

   void onRemoveFromContacts(bool)
   {
      qDebug() << __func__;
      if (!handler_){
         return;
      }
      handler_->onActionRemoveFromContacts(currentContact_);
   }

   void onAcceptFriendRequest(bool)
   {
      if (!handler_){
         return;
      }
      handler_->onActionAcceptContactRequest(currentContact_);
   }

   void onDeclineFriendRequest(bool)
   {
      if (!handler_){
         return;
      }
      handler_->onActionRejectContactRequest(currentContact_);
   }

   void prepareContactMenu()
   {
      if (!currentContact_){
         return;
      }

      switch (currentContact_->getContactStatus()) {
//         case Chat::ContactStatus::
//            addAction(tr("Add friend"), this, &ChatUsersContextMenu::onAddToContacts);
//            break;
         case Chat::ContactStatus::Accepted:
            addAction(tr("Remove from contacts"), this, &ChatUsersContextMenu::onRemoveFromContacts);
            break;
         case Chat::ContactStatus::Incoming:
            addAction(tr("Accept friend request"), this, &ChatUsersContextMenu::onAcceptFriendRequest);
            addAction(tr("Decline friend request"), this, &ChatUsersContextMenu::onDeclineFriendRequest);
            break;
         case Chat::ContactStatus::Outgoing:
            addAction(tr("This request is not accepted"));
            addAction(tr("Remove from contacts"), this, &ChatUsersContextMenu::onRemoveFromContacts);
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
   std::shared_ptr<Chat::ContactRecordData> currentContact_;
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
}

void ChatClientUserView::addWatcher(ViewItemWatcher * watcher)
{
   watchers_.push_back(watcher);
}

void ChatClientUserView::setActiveChatLabel(QLabel *label)
{
   label_ = label;
}

void ChatClientUserView::setCurrentUserChat(const QString &userId)
{
   // find all indexes
   QModelIndexList indexes = model()->match(model()->index(0,0),
                                            Qt::DisplayRole,
                                            QLatin1String("*"),
                                            -1,
                                            Qt::MatchWildcard|Qt::MatchRecursive);

   // set required chat
   for (auto index : indexes) {
      if (index.data(ChatClientDataModel::Role::ItemTypeRole).value<ChatUIDefinitions::ChatTreeNodeType>() == ChatUIDefinitions::ChatTreeNodeType::ContactsElement) {
         if (index.data(ChatClientDataModel::Role::ContactIdRole).toString() == userId) {
            setCurrentIndex(index);
            break;
         }
      }
   }
}

void ChatClientUserView::onCustomContextMenu(const QPoint & point)
{
   if (!contextMenu_) {
      if (handler_){
         contextMenu_ = new ChatUsersContextMenu(handler_.get(), this);
      }
   }
   if (contextMenu_){
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

void ChatClientUserView::updateDependUI(CategoryElement *element)
{
   auto data = static_cast<CategoryElement*>(element)->getDataObject();
   switch (element->getType()) {
      case ChatUIDefinitions::ChatTreeNodeType::RoomsElement:{
         std::shared_ptr<Chat::RoomData> room = std::dynamic_pointer_cast<Chat::RoomData>(data);
         if (label_){
            label_->setText(QObject::tr("CHAT #") + room->getId());
         }
      } break;
      case ChatUIDefinitions::ChatTreeNodeType::ContactsElement:{
         std::shared_ptr<Chat::ContactRecordData> contact = std::dynamic_pointer_cast<Chat::ContactRecordData>(data);
         if (label_){
            label_->setText(QObject::tr("CHAT #") + contact->getContactId());
         }
      } break;
      case ChatUIDefinitions::ChatTreeNodeType::AllUsersElement:{
         std::shared_ptr<Chat::UserData> room = std::dynamic_pointer_cast<Chat::UserData>(data);
         if (label_){
            label_->setText(QObject::tr("CHAT #") + room->getUserId());
         }
      } break;
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

void ChatClientUserView::notifyMessageChanged(std::shared_ptr<Chat::MessageData> message)
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

void ChatClientUserView::setHandler(std::shared_ptr<ChatItemActionsHandler> handler)
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
         }
         break;
         case ChatUIDefinitions::ChatTreeNodeType::RoomsElement:
         case ChatUIDefinitions::ChatTreeNodeType::ContactsElement:{
            auto node = static_cast<CategoryElement*>(item);
            notifyElementUpdated(node);
         }
         break;
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
