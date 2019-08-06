#include "ChatClientUsersViewItemDelegate.h"
#include "ChatClientDataModel.h"
#include <QPainter>
#include <QLineEdit>
#include "ChatProtocol/ChatUtils.h"

#include "ChatPartiesTreeModel.h"

namespace {
   const int kDotSize = 8;
   const QString kDotPathname = QLatin1String{ ":/ICON_DOT" };
   const QString unknown = QLatin1String{ "<unknown>" };

   // Delete below
   using Role = ChatClientDataModel::Role;
   using OnlineStatus = ChatContactElement::OnlineStatus;
   using ContactStatus = Chat::ContactStatus;
}

ChatClientUsersViewItemDelegate::ChatClientUsersViewItemDelegate(QObject *parent)
   : QStyledItemDelegate (parent)
{
}

// Previous code need to be deleted after done with styling
//void ChatClientUsersViewItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
//{
//   const ChatUIDefinitions::ChatTreeNodeType nodeType =
//            qvariant_cast<ChatUIDefinitions::ChatTreeNodeType>(index.data(Role::ItemTypeRole));

//   switch (nodeType) {
//      case ChatUIDefinitions::ChatTreeNodeType::CategoryGroupNode:
//         return paintCategoryNode(painter, option, index);
//      case ChatUIDefinitions::ChatTreeNodeType::RoomsElement:
//         return paintRoomsElement(painter, option, index);
//      case ChatUIDefinitions::ChatTreeNodeType::ContactsElement:
//      case ChatUIDefinitions::ChatTreeNodeType::ContactsRequestElement:
//         return paintContactsElement(painter, option, index);
//      case ChatUIDefinitions::ChatTreeNodeType::AllUsersElement:
//      case ChatUIDefinitions::ChatTreeNodeType::SearchElement:
//         return paintUserElement(painter, option, index);
//      default:
//         return QStyledItemDelegate::paint(painter, option, index);
//   }
//}

void ChatClientUsersViewItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
   AbstractParty* internalData = checkAndGetInternalPointer<AbstractParty>(index);

   if (UI::ElementType::Container == internalData->elementType()) {
      paintPartyContainer(painter, option, index);
   } else if (UI::ElementType::Party == internalData->elementType()) {
      paintParty(painter, option, index);
   } else {
      // You should specify rules for new ElementType explicitly
      Q_ASSERT(false);
   }
}

// paintCategoryNode == paintPartyContainer
void ChatClientUsersViewItemDelegate::paintCategoryNode(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
   QStyleOptionViewItem itemOption(option);

   if (itemOption.state & QStyle::State_Selected) {
      painter->save();
      painter->fillRect(itemOption.rect, itemStyle_.colorHighlightBackground());
      painter->restore();
   }

   itemOption.palette.setColor(QPalette::Text, itemStyle_.colorCategoryItem());

   itemOption.text = index.data(Role::CategoryGroupDisplayName).toString();

   QStyledItemDelegate::paint(painter, itemOption, index);
}

void ChatClientUsersViewItemDelegate::paintPartyContainer(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
   QStyleOptionViewItem itemOption(option);

   if (itemOption.state & QStyle::State_Selected) {
      painter->save();
      painter->fillRect(itemOption.rect, itemStyle_.colorHighlightBackground());
      painter->restore();
   }

   itemOption.palette.setColor(QPalette::Text, itemStyle_.colorCategoryItem());

   PartyContainer* container = checkAndGetInternalPointer<PartyContainer>(index);
   if (container) {
      itemOption.text = QString::fromStdString(container->getDisplayName());
   } else {
      itemOption.text = unknown;
   }

   QStyledItemDelegate::paint(painter, itemOption, index);
}

void ChatClientUsersViewItemDelegate::paintParty(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
   QStyleOptionViewItem itemOption(option);
   if (itemOption.state & QStyle::State_Selected) {
      painter->save();
      painter->fillRect(itemOption.rect, itemStyle_.colorHighlightBackground());
      painter->restore();
   }

   itemOption.palette.setColor(QPalette::Text, itemStyle_.colorRoom());
   Party* party = checkAndGetInternalPointer<Party>(index);
   PartyContainer* container = checkAndGetInternalPointer<PartyContainer>(index.parent());

   if (!party || !container) {
      QStyledItemDelegate::paint(painter, itemOption, index);
      return;
   }

   if (Chat::PartyType::GLOBAL == container->getPartyType()) {
      itemOption.text = QString::fromStdString(party->getDisplayName());
      QStyledItemDelegate::paint(painter, itemOption, index);
   } else if (Chat::PartyType::PRIVATE_DIRECT_MESSAGE == container->getPartyType()) {

      switch (party->getClientStatus()) {
      case Chat::ClientStatus::ONLINE: {
         itemOption.palette.setColor(QPalette::Text, itemStyle_.colorContactOnline());
         break;
      }
      case Chat::ClientStatus::OFFLINE: {
         itemOption.palette.setColor(QPalette::Text, itemStyle_.colorContactOffline());
         if (option.state & QStyle::State_Selected) {
            painter->save();
            painter->fillRect(itemOption.rect, itemStyle_.colorContactOffline());
            painter->restore();
         }
         break;
      }
      default: {
         // You should specify rules for new ClientStatus explicitly
         Q_ASSERT(false);
         break;
      }
      }


   } else {
      // You should specify rules for new PartyType explicitly
      Q_ASSERT(false);
   }

   QStyledItemDelegate::paint(painter, itemOption, index);

   // draw dot
   // Need new message indicator OTC and private messages
}

void ChatClientUsersViewItemDelegate::paintRoomsElement(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
   QStyleOptionViewItem itemOption(option);
   if (index.data(Role::ItemTypeRole).value<ChatUIDefinitions::ChatTreeNodeType>() != ChatUIDefinitions::ChatTreeNodeType::RoomsElement) {
      itemOption.text = QLatin1String("<unknown>");
      return QStyledItemDelegate::paint(painter, itemOption, index);
   }

   if (itemOption.state & QStyle::State_Selected) {
      painter->save();
      painter->fillRect(itemOption.rect, itemStyle_.colorHighlightBackground());
      painter->restore();
   }

   itemOption.palette.setColor(QPalette::Text, itemStyle_.colorRoom());
   const bool newMessage = index.data(Role::ChatNewMessageRole).toBool();
   const bool isGlobalRoom = (index.data(ChatClientDataModel::Role::RoomIdRole).toString().toStdString() == ChatUtils::GlobalRoomKey);
   const bool isSupportRoom = (index.data(ChatClientDataModel::Role::RoomIdRole).toString().toStdString() == ChatUtils::SupportRoomKey);
   itemOption.text = index.data(Role::RoomTitleRole).toString();
   QStyledItemDelegate::paint(painter, itemOption, index);

   // draw dot
   if (newMessage && !isGlobalRoom && !isSupportRoom) {
      QFontMetrics fm(itemOption.font, painter->device());
      auto textRect = fm.boundingRect(itemOption.rect, 0, itemOption.text);
      const QPixmap pixmap(kDotPathname);
      const QRect r(itemOption.rect.left() + textRect.width() + kDotSize,
                    itemOption.rect.top() + itemOption.rect.height() / 2 - kDotSize / 2 + 1,
                    kDotSize, kDotSize);
      painter->drawPixmap(r, pixmap, pixmap.rect());
   }
}

void ChatClientUsersViewItemDelegate::paintContactsElement(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
   QStyleOptionViewItem itemOption(option);
   if (index.data(Role::ItemTypeRole).value<ChatUIDefinitions::ChatTreeNodeType>() != ChatUIDefinitions::ChatTreeNodeType::ContactsElement
       && index.data(Role::ItemTypeRole).value<ChatUIDefinitions::ChatTreeNodeType>() != ChatUIDefinitions::ChatTreeNodeType::ContactsRequestElement) {
      itemOption.text = QLatin1String("<unknown>");
      return QStyledItemDelegate::paint(painter, itemOption, index);
   }

   ContactStatus contactStatus = index.data(Role::ContactStatusRole).value<ContactStatus>();
   OnlineStatus onlineStatus = index.data(Role::ContactOnlineStatusRole).value<OnlineStatus>();
   bool newMessage = index.data(Role::ChatNewMessageRole).toBool();
   itemOption.text = index.data(Role::ContactTitleRole).toString();

   if ((itemOption.state & QStyle::State_Selected) && contactStatus != Chat::CONTACT_STATUS_ACCEPTED) {
      painter->save();
      painter->fillRect(itemOption.rect, itemStyle_.colorHighlightBackground());
      painter->restore();
   }

   switch (contactStatus) {
      case ContactStatus::CONTACT_STATUS_ACCEPTED:
         //If accepted need to paint online status in the next switch
         break;
      case ContactStatus::CONTACT_STATUS_INCOMING:
         itemOption.palette.setColor(QPalette::Text, itemStyle_.colorContactIncoming());
         return QStyledItemDelegate::paint(painter, itemOption, index);
      case ContactStatus::CONTACT_STATUS_OUTGOING_PENDING:
      case ContactStatus::CONTACT_STATUS_OUTGOING:
         itemOption.palette.setColor(QPalette::Text, itemStyle_.colorContactOutgoing());
         return QStyledItemDelegate::paint(painter, itemOption, index);
      case ContactStatus::CONTACT_STATUS_REJECTED:
         itemOption.palette.setColor(QPalette::Text, itemStyle_.colorContactRejected());
         return QStyledItemDelegate::paint(painter, itemOption, index);
      default:
         return;
   }

   switch (onlineStatus) {
      case OnlineStatus::Online:
         itemOption.palette.setColor(QPalette::Text, itemStyle_.colorContactOnline());
         break;
      case OnlineStatus::Offline:
         itemOption.palette.setColor(QPalette::Text, itemStyle_.colorContactOffline());
         break;
   }

   if (option.state & QStyle::State_Selected) {
      painter->save();
      switch (onlineStatus) {
         case OnlineStatus::Online:
            painter->fillRect(itemOption.rect, itemStyle_.colorHighlightBackground());
            break;
         case OnlineStatus::Offline:
            painter->fillRect(itemOption.rect, itemStyle_.colorContactOffline());
            break;
      }
      painter->restore();
   }

   QStyledItemDelegate::paint(painter, itemOption, index);

   // draw dot
   if (newMessage) {
      painter->save();
      QFontMetrics fm(itemOption.font, painter->device());
      auto textRect = fm.boundingRect(itemOption.rect, 0, itemOption.text);
      const QPixmap pixmap(kDotPathname);
      const QRect r(itemOption.rect.left() + textRect.width() + kDotSize,
                    itemOption.rect.top() + itemOption.rect.height() / 2 - kDotSize / 2 + 1,
                    kDotSize, kDotSize);
      painter->drawPixmap(r, pixmap, pixmap.rect());
      painter->restore();
   }
}

void ChatClientUsersViewItemDelegate::paintUserElement(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
   QStyleOptionViewItem itemOption(option);
   if (index.data(Role::ItemTypeRole).value<ChatUIDefinitions::ChatTreeNodeType>() != ChatUIDefinitions::ChatTreeNodeType::AllUsersElement
       && index.data(Role::ItemTypeRole).value<ChatUIDefinitions::ChatTreeNodeType>() != ChatUIDefinitions::ChatTreeNodeType::SearchElement) {
      itemOption.text = QLatin1String("<unknown>");
      return QStyledItemDelegate::paint(painter, itemOption, index);
   }

   if (itemOption.state & QStyle::State_Selected) {
      painter->save();
      painter->fillRect(itemOption.rect, itemStyle_.colorHighlightBackground());
      painter->restore();
   }

   switch (index.data(Role::UserOnlineStatusRole).value<Chat::UserStatus>()) {
      case Chat::USER_STATUS_ONLINE:
         itemOption.palette.setColor(QPalette::Text, itemStyle_.colorUserOnline());
         break;
      case Chat::USER_STATUS_OFFLINE:
         itemOption.palette.setColor(QPalette::Text, itemStyle_.colorUserOffline());
         break;
      default:
         break;
   }
   itemOption.text = index.data(Role::UserIdRole).toString();
   QStyledItemDelegate::paint(painter, itemOption, index);
}

QWidget *ChatClientUsersViewItemDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
   QWidget * editor = QStyledItemDelegate::createEditor(parent, option, index);
   editor->setProperty("contact_editor", true);
   return editor;
}

template<typename AbstractPartySubClass>
AbstractPartySubClass* ChatClientUsersViewItemDelegate::checkAndGetInternalPointer(const QModelIndex &index) const
{
   AbstractPartySubClass* internalData = static_cast<AbstractPartySubClass*>(index.internalPointer());
#ifndef QT_NO_DEBUG
   Q_ASSERT(internalData);
#endif
   return internalData;
}
