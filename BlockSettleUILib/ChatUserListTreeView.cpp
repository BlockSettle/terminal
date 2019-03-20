#include "ChatUserListTreeView.h"

#include <QPainter>

namespace {
   static const int DOT_RADIUS = 4;
   static const int DOT_SHIFT = 8 + DOT_RADIUS;
}

ChatUserListTreeView::ChatUserListTreeView(QWidget *parent) : QTreeView(parent), internalStyle_(this)
{
   chatUserListModel_ = new ChatUserListTreeViewModel(this);
   setModel(chatUserListModel_);
   setItemDelegate(new ChatUserListTreeViewDelegate(internalStyle_, this));
 
   connect(this, &QAbstractItemView::clicked,
         this, &ChatUserListTreeView::onUserListItemClicked);
}

void ChatUserListTreeView::onChatUserDataListChanged(const ChatUserDataListPtr &chatUserDataList)
{
   chatUserListModel_->setChatUserDataList(chatUserDataList);
   expandAll();
}

void ChatUserListTreeView::onChatRoomDataListChanged(const Chat::ChatRoomDataListPtr &roomsDataList)
{
   chatUserListModel_->setChatRoomDataList(roomsDataList);
   expandAll();   
}

void ChatUserListTreeView::onUserListItemClicked(const QModelIndex &index)
{
   const auto &itemType =
            qvariant_cast<ChatUserListTreeViewModel::ItemType>(index.data(ChatUserListTreeViewModel::ItemTypeRole));

   clearSelection();

   if (itemType == ChatUserListTreeViewModel::ItemType::RoomItem) {
      const QString roomId = index.data(ChatUserListTreeViewModel::RoomIDRole).toString();
      emit roomClicked(roomId);

   }
   else if (itemType == ChatUserListTreeViewModel::ItemType::UserItem) {
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
            qvariant_cast<ChatUserListTreeViewModel::ItemType>(index.data(ChatUserListTreeViewModel::ItemTypeRole));

   QStyleOptionViewItem itemOption(option);

   if (itemType == ChatUserListTreeViewModel::ItemType::RoomItem) {
      itemOption.palette.setColor(QPalette::Text, internalStyle_.colorRoom());
      itemOption.palette.setColor(QPalette::HighlightedText, internalStyle_.colorRoom());
      QStyledItemDelegate::paint(painter, itemOption, index);
      
      // draw dot
      const bool haveMessage =
            qvariant_cast<bool>(index.data(ChatUserListTreeViewModel::HaveNewMessageRole));
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
   else if (itemType == ChatUserListTreeViewModel::ItemType::UserItem) {
      // set default text color
      itemOption.palette.setColor(QPalette::Text, internalStyle_.colorUserDefault());
      itemOption.palette.setColor(QPalette::HighlightedText, internalStyle_.colorUserDefault());

      const auto &userState =
            qvariant_cast<ChatUserData::State>(index.data(ChatUserListTreeViewModel::UserStateRole));

      // text color for friend request
      if (userState == ChatUserData::State::IncomingFriendRequest) {
         itemOption.palette.setColor(QPalette::Text, internalStyle_.colorIncomingFriendRequest());
         itemOption.palette.setColor(QPalette::HighlightedText, internalStyle_.colorIncomingFriendRequest());
         return QStyledItemDelegate::paint(painter, itemOption, index);
      }

      const auto &userStatus =
            qvariant_cast<ChatUserData::ConnectionStatus>(index.data(ChatUserListTreeViewModel::UserConnectionStatusRole));

      // text color for user online status
      if (userStatus == ChatUserData::ConnectionStatus::Online) {
         itemOption.palette.setColor(QPalette::Text, internalStyle_.colorUserOnline());
         itemOption.palette.setColor(QPalette::HighlightedText, internalStyle_.colorUserOnline());
      }

      QStyledItemDelegate::paint(painter, itemOption, index);

      // draw dot
      const bool haveMessage =
            qvariant_cast<bool>(index.data(ChatUserListTreeViewModel::HaveNewMessageRole));
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
