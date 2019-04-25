#include "ChatClientUserView.h"
#include "ChatClientTree/TreeObjects.h"
#include "ChatClientUsersViewItemDelegate.h"

#include <QMenu>

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
      currentIndex_ = view_->indexAt(point);

      clear();
      currentContact_.reset();
      //ItemType type = static_cast<ItemType>(currentIndex_.data(Role::ItemTypeRole).toInt());
      TreeItem * item = static_cast<TreeItem*>(currentIndex_.internalPointer());
      if (item->getType() == TreeItem::NodeType::ContactsElement) {
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
            addAction(tr("Remove friend"), this, &ChatUsersContextMenu::onRemoveFromContacts);
            break;
         case Chat::ContactStatus::Incoming:
            addAction(tr("Accept friend request"), this, &ChatUsersContextMenu::onAcceptFriendRequest);
            addAction(tr("Decline friend request"), this, &ChatUsersContextMenu::onDeclineFriendRequest);
            break;
         case Chat::ContactStatus::Outgoing:
            addAction(tr("This request is not accepted"));
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
}

void ChatClientUserView::addWatcher(ViewItemWatcher * watcher)
{
   watchers_.push_back(watcher);
}

void ChatClientUserView::setActiveChatLabel(QLabel *label)
{
   label_ = label;
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

void ChatClientUserView::updateDependUI(CategoryElement *element)
{
   auto data = static_cast<CategoryElement*>(element)->getDataObject();
   switch (element->getType()) {
      case TreeItem::NodeType::RoomsElement:{
         std::shared_ptr<Chat::RoomData> room = std::dynamic_pointer_cast<Chat::RoomData>(data);
         if (label_){
            label_->setText(QObject::tr("CHAT #") + room->getId());
         }
      } break;
      case TreeItem::NodeType::ContactsElement:{
         std::shared_ptr<Chat::ContactRecordData> contact = std::dynamic_pointer_cast<Chat::ContactRecordData>(data);
         if (label_){
            label_->setText(QObject::tr("CHAT #") + contact->getContactId());
         }
      } break;
      case TreeItem::NodeType::AllUsersElement:{
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
   TreeItem* item = static_cast<TreeItem*>(current.internalPointer());
   if (!watchers_.empty() && item) {
      switch (item->getType()) {
         case TreeItem::NodeType::RoomsElement:
         case TreeItem::NodeType::ContactsElement:
         case TreeItem::NodeType::AllUsersElement:{
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
      TreeItem* item = static_cast<TreeItem*>(topLeft.internalPointer());
      switch (item->getType()) {
         case TreeItem::NodeType::MessageDataNode: {
            auto mnode = static_cast<TreeMessageNode*>(item);
            notifyMessageChanged(mnode->getMessage());
         }
         case TreeItem::NodeType::RoomsElement:
         case TreeItem::NodeType::ContactsElement:{
            auto node = static_cast<CategoryElement*>(item);
            notifyElementUpdated(node);
         }
         default:
            break;
      }
   }
}

void LoggerWatcher::onElementSelected(CategoryElement *element)
{
   qDebug() << "Item selected:\n" << QString::fromStdString(element->getDataObject()->toJsonString());
}

void LoggerWatcher::onElementUpdated(CategoryElement *element)
{
   qDebug() << "Item updated:\n" << QString::fromStdString(element->getDataObject()->toJsonString());
}

void LoggerWatcher::onMessageChanged(std::shared_ptr<Chat::MessageData> message)
{
   qDebug() << "Message changed:\n" << QString::fromStdString(message->toJsonString());
}
