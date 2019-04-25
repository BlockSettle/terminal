#include "ChatClientUsersViewItemDelegate.h"
#include "ChatClientDataModel.h"

using NodeType = TreeItem::NodeType;
using Role = ChatClientDataModel::Role;
using OnlineStatus = ChatContactElement::OnlineStatus;

ChatClientUsersViewItemDelegate::ChatClientUsersViewItemDelegate(QObject *parent)
: QStyledItemDelegate(parent)
{

}

void ChatClientUsersViewItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
   const NodeType nodeType =
            qvariant_cast<NodeType>(index.data(Role::ItemTypeRole));

   switch (nodeType) {
      case NodeType::CategoryNode:
         return paintCategoryNode(painter, option, index);
      case NodeType::RoomsElement:
         return paintRoomsElement(painter, option, index);
      case NodeType::ContactsElement:
         return paintContactsElement(painter, option, index);
      case NodeType::AllUsersElement:
      case NodeType::SearchElement:
         return paintUserElement(painter, option, index);
      default:
         return QStyledItemDelegate::paint(painter, option, index);
   }


}

void ChatClientUsersViewItemDelegate::paintCategoryNode(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
   QStyleOptionViewItem itemOption(option);
   itemOption.palette.setColor(QPalette::Text, Qt::gray);
   itemOption.palette.setColor(QPalette::HighlightedText, Qt::gray);
   switch (index.data(Role::ItemAcceptTypeRole).value<NodeType>()){
      case TreeItem::NodeType::SearchElement:
         itemOption.text = QLatin1String("Search");
         break;
      case TreeItem::NodeType::RoomsElement:
         itemOption.text = QLatin1String("Chat rooms");
         break;
      case TreeItem::NodeType::ContactsElement:
         itemOption.text = QLatin1String("Contacts");
         break;
      case TreeItem::NodeType::AllUsersElement:
         itemOption.text = QLatin1String("Users");
         break;
      default:
         itemOption.text = QLatin1String("<unknown>");

   }

   QStyledItemDelegate::paint(painter, itemOption, index);

}

void ChatClientUsersViewItemDelegate::paintRoomsElement(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
   QStyleOptionViewItem itemOption(option);
   if (index.data(Role::ItemTypeRole).value<NodeType>() != NodeType::RoomsElement){
      itemOption.text = QLatin1String("<unknown>");
      return QStyledItemDelegate::paint(painter, itemOption, index);
   }

   itemOption.palette.setColor(QPalette::Text, QColor(0x00c8f8));
   //itemOption.palette.setColor(QPalette::Highlight, Qt::cyan);
   itemOption.text = index.data(Role::RoomIdRole).toString();
   QStyledItemDelegate::paint(painter, itemOption, index);

}

void ChatClientUsersViewItemDelegate::paintContactsElement(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
   QStyleOptionViewItem itemOption(option);
   if (index.data(Role::ItemTypeRole).value<NodeType>() != NodeType::ContactsElement){
      itemOption.text = QLatin1String("<unknown>");
      return QStyledItemDelegate::paint(painter, itemOption, index);
   }

   itemOption.palette.setColor(QPalette::Highlight, Qt::blue);
   switch (index.data(Role::ContactOnlineStatusRole).value<OnlineStatus>()) {
      case OnlineStatus::Online:
         itemOption.palette.setColor(QPalette::Text, QColor(0x00c8f8));
         break;
      case OnlineStatus::Offline:
         itemOption.palette.setColor(QPalette::Text, QColor(0xc0c0c0));
         break;
   }
   itemOption.text = index.data(Role::ContactIdRole).toString();
   QStyledItemDelegate::paint(painter, itemOption, index);
}

void ChatClientUsersViewItemDelegate::paintUserElement(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
   QStyleOptionViewItem itemOption(option);
   if (index.data(Role::ItemTypeRole).value<NodeType>() != NodeType::AllUsersElement
       && index.data(Role::ItemTypeRole).value<NodeType>() != NodeType::SearchElement) {
      itemOption.text = QLatin1String("<unknown>");
      return QStyledItemDelegate::paint(painter, itemOption, index);
   }

   itemOption.palette.setColor(QPalette::Text, Qt::cyan);
   itemOption.palette.setColor(QPalette::Highlight, Qt::blue);
   itemOption.text = index.data(Role::UserIdRole).toString();
   QStyledItemDelegate::paint(painter, itemOption, index);
}
