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

ChatUserCategoryListViewDelegate::ChatUserCategoryListViewDelegate
   (const ChatUserCategoryListViewStyle &style, QObject *parent)
   : QStyledItemDelegate (parent), internalStyle_(style)
{

}

void ChatUserCategoryListViewDelegate::paint(QPainter *painter,
   const QStyleOptionViewItem &option,
   const QModelIndex &index) const
{
   QStyleOptionViewItem itemOption(option);

   // set default text color
   itemOption.palette.setColor(QPalette::Text, internalStyle_.colorUserDefault());
   itemOption.palette.setColor(QPalette::HighlightedText, internalStyle_.colorUserDefault());

   ChatUserData::State userState =
         qvariant_cast<ChatUserData::State>(index.data(ChatUsersViewModel::UserStateRole));

   // text color for friend request
   if (userState == ChatUserData::State::IncomingFriendRequest)
   {
      itemOption.palette.setColor(QPalette::Text, internalStyle_.colorIncomingFriendRequest());
      itemOption.palette.setColor(QPalette::HighlightedText, internalStyle_.colorIncomingFriendRequest());
      return QStyledItemDelegate::paint(painter, itemOption, index);
   }

   ChatUserData::ConnectionStatus userStatus =
         qvariant_cast<ChatUserData::ConnectionStatus>(index.data(ChatUsersViewModel::UserConnectionStatusRole));

   // text color for user online status
   if (userStatus == ChatUserData::ConnectionStatus::Online)
   {
      itemOption.palette.setColor(QPalette::Text, internalStyle_.colorUserOnline());
      itemOption.palette.setColor(QPalette::HighlightedText, internalStyle_.colorUserOnline());
   }

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
      painter->setBrush(QBrush(internalStyle_.colorUserOnline(), Qt::SolidPattern));
      painter->drawEllipse(dotPoint, DOT_RADIUS, DOT_RADIUS);
      painter->restore();
   }
}

ChatUserCategoryListView::ChatUserCategoryListView(QWidget *parent) : QListView(parent), internalStyle_(this)
{
   setItemDelegate(new ChatUserCategoryListViewDelegate(internalStyle_, this));
}
