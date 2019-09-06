#include "ChatClientUsersViewItemDelegate.h"
#include <QPainter>
#include <QLineEdit>

namespace {
   const int kDotSize = 8;
   const QString kDotPathname = QLatin1String{ ":/ICON_DOT" };
}

ChatClientUsersViewItemDelegate::ChatClientUsersViewItemDelegate(ChatPartiesSortProxyModelPtr proxyModel, QObject *parent)
   : QStyledItemDelegate (parent)
   , proxyModel_(proxyModel)
{
}

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
   if (itemOption.state & QStyle::State_Selected) {
      painter->save();
      painter->fillRect(itemOption.rect, itemStyle_.colorHighlightBackground());
      painter->restore();
   }

   itemOption.palette.setColor(QPalette::Text, itemStyle_.colorRoom());
   PartyTreeItem* party = static_cast<PartyTreeItem*>(index.internalPointer());
   Chat::ClientPartyPtr clientPartyPtr = party->data().value<Chat::ClientPartyPtr>();

   itemOption.text = QString::fromStdString(clientPartyPtr->displayName());
   if (clientPartyPtr->isPrivateStandard()) {
      if (Chat::PartyState::INITIALIZED == clientPartyPtr->partyState()) {
         paintInitParty(clientPartyPtr, painter, itemOption);
      }
      else {
         paintRequestParty(clientPartyPtr, painter, itemOption);
      }
   }

   QStyledItemDelegate::paint(painter, itemOption, index);

   if (party->hasNewMessages()) {
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

void ChatClientUsersViewItemDelegate::paintInitParty(Chat::ClientPartyPtr& clientPartyPtr, QPainter* painter,
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
         if (itemOption.state & QStyle::State_Selected) {
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

void ChatClientUsersViewItemDelegate::paintRequestParty(Chat::ClientPartyPtr& clientPartyPtr, QPainter* painter,
   QStyleOptionViewItem& itemOption) const
{
   // #new_logic : do not forget to add color for outgoing and incoming requests
   switch (clientPartyPtr->partyState()) {
   case Chat::PartyState::UNINITIALIZED:
      itemOption.palette.setColor(QPalette::Text, itemStyle_.colorContactOutgoing());
      break;
   case Chat::PartyState::REQUESTED:
      if (clientPartyPtr->partyCreatorHash() == proxyModel_->currentUser()) {
         itemOption.palette.setColor(QPalette::Text, itemStyle_.colorContactOutgoing());
      }
      else {
         itemOption.palette.setColor(QPalette::Text, itemStyle_.colorContactIncoming());
      }
      break;
   case Chat::PartyState::REJECTED:
      itemOption.palette.setColor(QPalette::Text, itemStyle_.colorContactRejected());
      break;
   default:
      break;
   }
}

QWidget *ChatClientUsersViewItemDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
   QWidget * editor = QStyledItemDelegate::createEditor(parent, option, index);
   editor->setProperty("contact_editor", true);
   return editor;
}
