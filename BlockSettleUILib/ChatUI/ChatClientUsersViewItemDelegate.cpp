/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
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
   if (!clientPartyPtr) {
      return;
   }

   itemOption.text = QString::fromStdString(clientPartyPtr->displayName());
   if (clientPartyPtr->isPrivateStandard() || clientPartyPtr->isPrivateOTC()) {
      if (Chat::PartyState::INITIALIZED == clientPartyPtr->partyState()) {
         paintInitParty(party, painter, itemOption);
      }
      else {
         paintRequestParty(clientPartyPtr, painter, itemOption);
      }
   }

   QStyledItemDelegate::paint(painter, itemOption, index);

   if (party->hasNewMessages() && Chat::PartyState::REQUESTED != clientPartyPtr->partyState()) {
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

void ChatClientUsersViewItemDelegate::paintInitParty(PartyTreeItem* partyTreeItem, QPainter* painter,
   QStyleOptionViewItem& itemOption) const
{
   Chat::ClientPartyPtr clientPartyPtr = partyTreeItem->data().value<Chat::ClientPartyPtr>();
   // This should be always true as far as we checked it in previous flow function
   assert(clientPartyPtr);

   switch (clientPartyPtr->clientStatus())
   {
      case Chat::ClientStatus::ONLINE:
      {
         QColor palleteColor = itemStyle_.colorContactOnline();
         if (partyTreeItem->isOTCTogglingMode() && !partyTreeItem->activeOTCToggleState()) {
            palleteColor = itemStyle_.colorContactOffline();
         }

         itemOption.palette.setColor(QPalette::Text, palleteColor);
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
