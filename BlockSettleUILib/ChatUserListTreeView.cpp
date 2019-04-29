#include "ChatUserListTreeView.h"

#include <QPainter>
#include <QMenu>
#include <QDebug>

namespace {
   static const int DOT_RADIUS = 4;
   static const int DOT_SHIFT = 8 + DOT_RADIUS;
}

using ItemType = ChatUserListTreeViewModel::ItemType;
using Role = ChatUserListTreeViewModel::Role;

class BSContextMenu : public QMenu
{
public:
   BSContextMenu(ChatUserListTreeView* view) : 
      QMenu(view),
      view_(view)
   {
      connect(this, &QMenu::aboutToHide, this, &BSContextMenu::clearMenu);
   }

   ~BSContextMenu()
   {
      qDebug() << __func__;
   }

   QAction* execMenu(const QPoint & point)
   {
      currentIndex_ = view_->indexAt(point);
      
      clear();

      ItemType type = static_cast<ItemType>(currentIndex_.data(Role::ItemTypeRole).toInt());
      if (type == ItemType::UserItem) {
         prepareUserMenu();
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
   }

   void onRemoveFromContacts(bool) 
   {
      qDebug() << __func__;
   }

   void onAcceptFriendRequest(bool)
   {
      const auto &text = currentIndex_.data(Qt::DisplayRole).toString();
      emit view_->acceptFriendRequest(text);
   }

   void onDeclineFriendRequest(bool)
   {
      const auto &text = currentIndex_.data(Qt::DisplayRole).toString();
      emit view_->declineFriendRequest(text);
   }

   ChatUserData::State userState()
   {
      return qvariant_cast<ChatUserData::State>(currentIndex_.data(Role::UserStateRole));
   }

   void prepareUserMenu()
   {
      switch (userState()) {
         case ChatUserData::State::Unknown:
            addAction(tr("Add friend"), this, &BSContextMenu::onAddToContacts);
            break;
         case ChatUserData::State::Friend:
            addAction(tr("Remove friend"), this, &BSContextMenu::onRemoveFromContacts);
            break;
         case ChatUserData::State::IncomingFriendRequest:
            addAction(tr("Accept friend request"), this, &BSContextMenu::onAcceptFriendRequest);
            addAction(tr("Decline friend request"), this, &BSContextMenu::onDeclineFriendRequest);
            break;
         case ChatUserData::State::OutgoingFriendRequest:
            addAction(tr("This request not accepted"));
         default:
            break;

      }
   }

   void prepareRoomMenu()
   {
      
   }

private:
   ChatUserListTreeView* view_;
   QModelIndex currentIndex_;
};

ChatUserListTreeView::ChatUserListTreeView(QWidget *parent) : QTreeView(parent), internalStyle_(this)
{
   chatUserListModel_ = new ChatUserListTreeViewModel(this);
   setModel(chatUserListModel_);
   setItemDelegate(new ChatUserListTreeViewDelegate(internalStyle_, this));
 
   connect(this, &QAbstractItemView::clicked,
         this, &ChatUserListTreeView::onUserListItemClicked);

   contextMenu_ = new BSContextMenu(this);
   setContextMenuPolicy(Qt::CustomContextMenu);
   connect(this, &QAbstractItemView::customContextMenuRequested, this, &ChatUserListTreeView::onCustomContextMenu);
}

void ChatUserListTreeView::selectFirstRoom()
{
   chatUserListModel_->selectFirstRoom();
}

void ChatUserListTreeView::highlightUserItem(const QString& userId)
{   
   chatUserListModel_->highlightUserItem(userId);
}   

void ChatUserListTreeView::onChatUserDataChanged(const ChatUserDataPtr &chatUserDataPtr)
{
   chatUserListModel_->setChatUserData(chatUserDataPtr);
   expandAll();
}

void ChatUserListTreeView::onChatUserDataListChanged(const ChatUserDataListPtr &chatUserDataList)
{
   chatUserListModel_->setChatUserDataList(chatUserDataList);
   expandAll();
}

void ChatUserListTreeView::onChatRoomDataChanged(const Chat::RoomDataPtr &roomsDataPtr)
{
   chatUserListModel_->setChatRoomData(roomsDataPtr);
   expandAll();
}

void ChatUserListTreeView::onChatRoomDataListChanged(const Chat::RoomDataListPtr &roomsDataList)
{
   chatUserListModel_->setChatRoomDataList(roomsDataList);
   expandAll();
}

void ChatUserListTreeView::onCustomContextMenu(const QPoint & point)
{
   contextMenu_->execMenu(point);
}

void ChatUserListTreeView::onUserListItemClicked(const QModelIndex &index)
{
   const auto &itemType =
            qvariant_cast<ItemType>(index.data(Role::ItemTypeRole));

   if (itemType == ItemType::RoomItem) {
      const QString roomId = index.data(Role::RoomIDRole).toString();
      emit roomClicked(roomId);

   }
   else if (itemType == ItemType::UserItem) {
      const QString userId = index.data(Qt::DisplayRole).toString();
      emit userClicked(userId);
   }
}

ChatUserListTreeViewModel *ChatUserListTreeView::chatUserListModel() const
{
   return chatUserListModel_;
}

ChatUserListTreeViewDelegate::ChatUserListTreeViewDelegate(const ChatUserListTreeViewStyle& style, QObject* parent)
: QStyledItemDelegate (parent), internalStyle_(style)
{
    
}

void ChatUserListTreeViewDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
   const auto &itemType =
            qvariant_cast<ItemType>(index.data(Role::ItemTypeRole));

   QStyleOptionViewItem itemOption(option);

   if (itemType == ItemType::RoomItem) {
      itemOption.palette.setColor(QPalette::Text, internalStyle_.colorRoom());
      itemOption.palette.setColor(QPalette::HighlightedText, internalStyle_.colorRoom());
      QStyledItemDelegate::paint(painter, itemOption, index);
      
      // draw dot
      const bool haveMessage =
            qvariant_cast<bool>(index.data(Role::HaveNewMessageRole));
      if (haveMessage) {
         auto text = index.data(Qt::DisplayRole).toString();
         QFontMetrics fm(itemOption.font, painter->device());
         auto textRect = fm.boundingRect(itemOption.rect, 0, text);
         auto textWidth = textRect.width();
         QPoint dotPoint(itemOption.rect.left() + textWidth + DOT_SHIFT, itemOption.rect.top() + itemOption.rect.height() / 2);
         painter->save();
         painter->setBrush(QBrush(internalStyle_.colorUserOnline(), Qt::SolidPattern));
         painter->drawEllipse(dotPoint, DOT_RADIUS, DOT_RADIUS);
         painter->restore();
      }

   } 
   else if (itemType == ItemType::UserItem) {
      // set default text color
      itemOption.palette.setColor(QPalette::Text, internalStyle_.colorUserDefault());
      itemOption.palette.setColor(QPalette::HighlightedText, internalStyle_.colorUserDefault());

      const auto &userState =
            qvariant_cast<ChatUserData::State>(index.data(Role::UserStateRole));

      // text color for friend request
      if (userState == ChatUserData::State::IncomingFriendRequest) {
         itemOption.palette.setColor(QPalette::Text, internalStyle_.colorIncomingFriendRequest());
         itemOption.palette.setColor(QPalette::HighlightedText, internalStyle_.colorIncomingFriendRequest());
         return QStyledItemDelegate::paint(painter, itemOption, index);
      } else if (userState == ChatUserData::State::OutgoingFriendRequest) {
         itemOption.palette.setColor(QPalette::Text, internalStyle_.colorOutgoingFriendRequest());
         itemOption.palette.setColor(QPalette::HighlightedText, internalStyle_.colorOutgoingFriendRequest());
         return QStyledItemDelegate::paint(painter, itemOption, index);
      }

      const auto &userStatus =
            qvariant_cast<ChatUserData::ConnectionStatus>(index.data(Role::UserConnectionStatusRole));

      // text color for user online status
      if (userStatus == ChatUserData::ConnectionStatus::Online) {
         itemOption.palette.setColor(QPalette::Text, internalStyle_.colorUserOnline());
         itemOption.palette.setColor(QPalette::HighlightedText, internalStyle_.colorUserOnline());
      }

      QStyledItemDelegate::paint(painter, itemOption, index);

      // draw dot
      const bool haveMessage =
            qvariant_cast<bool>(index.data(Role::HaveNewMessageRole));
      if (haveMessage) {
         auto text = index.data(Qt::DisplayRole).toString();
         QFontMetrics fm(itemOption.font, painter->device());
         auto textRect = fm.boundingRect(itemOption.rect, 0, text);
         auto textWidth = textRect.width();
         QPoint dotPoint(itemOption.rect.left() + textWidth + DOT_SHIFT, itemOption.rect.top() + itemOption.rect.height() / 2);
         painter->save();
         painter->setBrush(QBrush(internalStyle_.colorUserOnline(), Qt::SolidPattern));
         painter->drawEllipse(dotPoint, DOT_RADIUS, DOT_RADIUS);
         painter->restore();
      }
   } 
   else {
      
      QStyledItemDelegate::paint(painter, itemOption, index);
   }
   
}
