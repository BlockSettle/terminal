#include "ChatUserCategoryListView.h"

#include <QPainter>
#include <QStyleOptionViewItem>
#include <QModelIndex>
#include <QApplication>
#include <QFont>
#include <QPalette>

#include "ChatUserData.h"
#include "ChatUsersViewModel.h"

namespace {
   static const int DOT_RADIUS = 4;
   static const int DOT_SHIFT = 8 + DOT_RADIUS;
}

ChatUserCategoryListViewDelegate::ChatUserCategoryListViewDelegate(QObject *parent)
   : QStyledItemDelegate (parent)
{

}

void ChatUserCategoryListViewDelegate::paint(QPainter *painter,
   const QStyleOptionViewItem &option,
   const QModelIndex &index) const
{
   QStyleOptionViewItem itemOption(option);

   // set default text color
   itemOption.palette.setColor(QPalette::Text, QColor(ChatUserColor::COLOR_USER_DEFAULT));
   itemOption.palette.setColor(QPalette::HighlightedText, QColor(ChatUserColor::COLOR_USER_DEFAULT));

   ChatUserData::State userState =
         qvariant_cast<ChatUserData::State>(index.data(ChatUsersViewModel::UserStateRole));

   // text color for friend request
   if (userState == ChatUserData::State::IncomingFriendRequest)
   {
      itemOption.palette.setColor(QPalette::HighlightedText, QColor(ChatUserColor::COLOR_INCOMING_FRIEND_REQUEST));
      return QStyledItemDelegate::paint(painter, itemOption, index);
   }

   ChatUserData::ConnectionStatus userStatus =
         qvariant_cast<ChatUserData::ConnectionStatus>(index.data(ChatUsersViewModel::UserConnectionStatusRole));

   // text color for user online status
   if (userStatus == ChatUserData::ConnectionStatus::Online)
   {
      itemOption.palette.setColor(QPalette::HighlightedText, QColor(ChatUserColor::COLOR_USER_ONLINE));
      QStyledItemDelegate::paint(painter, itemOption, index);

      // draw dot
      bool haveMessage =
            qvariant_cast<bool>(index.data(ChatUsersViewModel::HaveNewMessageRole));
      if (haveMessage)
      {
         auto text = index.data(Qt::DisplayRole).toString();
         QFontMetrics fm(itemOption.font, painter->device());
         auto textRect = fm.boundingRect(itemOption.rect, 0, text);
         auto textWidth = textRect.width();
         QPoint dotPoint(textWidth + DOT_SHIFT, textRect.height()/2 + (itemOption.rect.height() * index.row()));
         painter->save();
         painter->setBrush(QBrush(QColor(ChatUserColor::COLOR_USER_ONLINE), Qt::SolidPattern));
         painter->drawEllipse(dotPoint, DOT_RADIUS, DOT_RADIUS);
         painter->restore();
      }

      return;
   }

   return QStyledItemDelegate::paint(painter, itemOption, index);
}

ChatUserCategoryListView::ChatUserCategoryListView(QWidget *parent) : QListView(parent)
{
   setItemDelegate(new ChatUserCategoryListViewDelegate(this));
}
