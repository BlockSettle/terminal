#include "ChatUsersViewItemStyle.h"

ChatUsersViewItemStyle::ChatUsersViewItemStyle(QWidget *parent)
   : QWidget(parent)
   , colorRoom_(Qt::white)
   , colorUserOnline_(Qt::white)
   , colorUserOffline_(Qt::gray)
   , colorContactOnline_(Qt::white)
   , colorContactOffline_(Qt::gray)
   , colorContactIncoming_(Qt::darkYellow)
   , colorContactOutgoing_(Qt::darkGreen)
   , colorContactRejected_(Qt::darkRed)
   , colorHighlightBackground_(Qt::cyan)
{

}

QColor ChatUsersViewItemStyle::colorCategoryItem() const
{
   return colorCategoryItem_;
}

QColor ChatUsersViewItemStyle::colorRoom() const
{
   return colorRoom_;
}

QColor ChatUsersViewItemStyle::colorUserOnline() const
{
   return colorUserOnline_;
}

QColor ChatUsersViewItemStyle::colorUserOffline() const
{
   return colorUserOffline_;
}

QColor ChatUsersViewItemStyle::colorContactOnline() const
{
   return colorContactOnline_;
}

QColor ChatUsersViewItemStyle::colorContactOffline() const
{
   return colorContactOffline_;
}

QColor ChatUsersViewItemStyle::colorContactIncoming() const
{
   return colorContactIncoming_;
}

QColor ChatUsersViewItemStyle::colorContactOutgoing() const
{
   return colorContactOutgoing_;
}

QColor ChatUsersViewItemStyle::colorContactRejected() const
{
   return colorContactRejected_;
}

QColor ChatUsersViewItemStyle::colorHighlightBackground() const
{
   return colorHighlightBackground_;
}

void ChatUsersViewItemStyle::setColorCategoryItem(QColor colorCategoryItem)
{
   colorCategoryItem_ = colorCategoryItem;
}

void ChatUsersViewItemStyle::setColorRoom(QColor colorRoom)
{
   colorRoom_ = colorRoom;
}

void ChatUsersViewItemStyle::setColorUserOnline(QColor colorUserOnline)
{
   colorUserOnline_ = colorUserOnline;
}

void ChatUsersViewItemStyle::setColorUserOffline(QColor colorUserOffline)
{
   colorUserOffline_ = colorUserOffline;
}

void ChatUsersViewItemStyle::setColorContactOnline(QColor colorContactOnline)
{
   colorContactOnline_ = colorContactOnline;
}

void ChatUsersViewItemStyle::setColorContactOffline(QColor colorContactOffline)
{
   colorContactOffline_ = colorContactOffline;
}

void ChatUsersViewItemStyle::setColorContactIncoming(QColor colorContactIncoming)
{
   colorContactIncoming_ = colorContactIncoming;
}

void ChatUsersViewItemStyle::setColorContactOutgoing(QColor colorContactOutgoing)
{
   colorContactOutgoing_ = colorContactOutgoing;
}

void ChatUsersViewItemStyle::setColorContactRejected(QColor colorContactRejected)
{
   colorContactRejected_ = colorContactRejected;
}

void ChatUsersViewItemStyle::setColorHighlightBackground(QColor colorHighlightBackground)
{
   colorHighlightBackground_ = colorHighlightBackground;
}
