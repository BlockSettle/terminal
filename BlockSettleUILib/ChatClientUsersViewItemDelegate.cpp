#include "ChatClientUsersViewItemDelegate.h"
#include "ChatClientDataModel.h"
#include <QPainter>
#include <QLineEdit>
#include "ChatProtocol/ChatUtils.h"

namespace {
   const int kDotSize = 8;
   const QString kDotPathname = QLatin1String{ ":/ICON_DOT" };
   const QString unknown = QLatin1String{ "<unknown>" };

   // Delete below #old_logic
   using Role = ChatClientDataModel::Role;
   using OnlineStatus = ChatContactElement::OnlineStatus;
   using ContactStatus = Chat::ContactStatus;
}

ChatClientUsersViewItemDelegate::ChatClientUsersViewItemDelegate(ChatPartiesSortProxyModelPtr proxyModel, QObject *parent)
   : QStyledItemDelegate (parent)
   , proxyModel_(proxyModel)
{
}

// #old_logic


void ChatClientUsersViewItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
   if (!index.isValid()) {
      return;
   }

   const QModelIndex& sourceIndex = proxyModel_ ? proxyModel_->mapToSource(index) : index;
   if (!sourceIndex.isValid()) {
      return;
   }

   PartyTreeItem* internalData = static_cast<PartyTreeItem*>(sourceIndex.internalPointer());
   if (internalData->modelType() == UI::ElementType::Container) {
      paintPartyContainer(painter, option, sourceIndex);
   }
   else if (internalData->modelType() == UI::ElementType::Party) {
      paintParty(painter, option, sourceIndex);
   }
}

// #new_logic
void ChatClientUsersViewItemDelegate::paintPartyContainer(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
   QStyleOptionViewItem itemOption(option);

   if (itemOption.state & QStyle::State_Selected) {
      painter->save();
      painter->fillRect(itemOption.rect, itemStyle_.colorHighlightBackground());
      painter->restore();
   }

   itemOption.palette.setColor(QPalette::Text, itemStyle_.colorCategoryItem());
   PartyTreeItem* internalData = static_cast<PartyTreeItem*>(index.internalPointer());
   Q_ASSERT(internalData && internalData->data().canConvert<QString>());
   itemOption.text = internalData->data().toString();

   QStyledItemDelegate::paint(painter, itemOption, index);
}

void ChatClientUsersViewItemDelegate::paintParty(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
   QStyleOptionViewItem itemOption(option);
   if (itemOption.state & QStyle::State_Selected)
   {
      painter->save();
      painter->fillRect(itemOption.rect, itemStyle_.colorHighlightBackground());
      painter->restore();
   }

   itemOption.palette.setColor(QPalette::Text, itemStyle_.colorRoom());
   PartyTreeItem* party = static_cast<PartyTreeItem*>(index.internalPointer());
   Chat::ClientPartyPtr clientPartyPtr = party->data().value<Chat::ClientPartyPtr>();

   itemOption.text = QString::fromStdString(clientPartyPtr->displayName());
   if (clientPartyPtr->isPrivateStandard())
   {
      if (Chat::PartyState::INITIALIZED == clientPartyPtr->partyState()) {
         paintInitParty(clientPartyPtr, painter, itemOption);
      }
      else {
         paintRequestParty(clientPartyPtr, painter, itemOption);
      }
   }

   QStyledItemDelegate::paint(painter, itemOption, index);

   // #new_logic: draw dot
   // Need new message indicator OTC and private messages
}

void ChatClientUsersViewItemDelegate::paintInitParty(Chat::ClientPartyPtr clientPartyPtr, QPainter* painter,
   QStyleOptionViewItem& itemOption) const
{
   switch (clientPartyPtr->clientStatus())
   {
      case Chat::ClientStatus::ONLINE:
      {
         itemOption.palette.setColor(QPalette::Text, itemStyle_.colorContactOnline());
      }
      break;

      case Chat::ClientStatus::OFFLINE:
      {
         itemOption.palette.setColor(QPalette::Text, itemStyle_.colorContactOffline());
         if (itemOption.state & QStyle::State_Selected)
         {
            painter->save();
            painter->fillRect(itemOption.rect, itemStyle_.colorContactOffline());
            painter->restore();
         }
      }
      break;

      default: 
      {
         // You should specify rules for new ClientStatus explicitly
         Q_ASSERT(false);
      }
      break;
   }
}

void ChatClientUsersViewItemDelegate::paintRequestParty(Chat::ClientPartyPtr clientPartyPtr, QPainter* painter,
   QStyleOptionViewItem& itemOption) const
{
   // #new_logic : do not forget to add color for outgoing and incoming requests
   switch (clientPartyPtr->partyState()) {
   case Chat::PartyState::UNINITIALIZED:
      itemOption.palette.setColor(QPalette::Text, itemStyle_.colorContactOutgoing());
      break;
   case Chat::PartyState::REQUESTED:
      if (clientPartyPtr->senderId() == proxyModel_->currentUser()) {
         itemOption.palette.setColor(QPalette::Text, itemStyle_.colorContactOutgoing());
         break;
      }
      else {
         itemOption.palette.setColor(QPalette::Text, itemStyle_.colorContactIncoming());
         break;
      }
   case Chat::PartyState::REJECTED:
      itemOption.palette.setColor(QPalette::Text, itemStyle_.colorContactRejected());
      break;
   default:
      break;
   }
}

// #old_logic
/*
// Previous code need to be deleted after done with styling
void ChatClientUsersViewItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
   const ChatUIDefinitions::ChatTreeNodeType nodeType =
            qvariant_cast<ChatUIDefinitions::ChatTreeNodeType>(index.data(Role::ItemTypeRole));

   switch (nodeType) {
      case ChatUIDefinitions::ChatTreeNodeType::CategoryGroupNode:
         return paintCategoryNode(painter, option, index);
      case ChatUIDefinitions::ChatTreeNodeType::RoomsElement:
         return paintRoomsElement(painter, option, index);
      case ChatUIDefinitions::ChatTreeNodeType::ContactsElement:
      case ChatUIDefinitions::ChatTreeNodeType::ContactsRequestElement:
         return paintContactsElement(painter, option, index);
      case ChatUIDefinitions::ChatTreeNodeType::AllUsersElement:
      case ChatUIDefinitions::ChatTreeNodeType::SearchElement:
         return paintUserElement(painter, option, index);
      default:
         return QStyledItemDelegate::paint(painter, option, index);
   }
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
*/

QWidget *ChatClientUsersViewItemDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
   QWidget * editor = QStyledItemDelegate::createEditor(parent, option, index);
   editor->setProperty("contact_editor", true);
   return editor;
}
