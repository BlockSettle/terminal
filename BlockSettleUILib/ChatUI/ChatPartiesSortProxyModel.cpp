/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "ChatPartiesSortProxyModel.h"

ChatPartiesSortProxyModel::ChatPartiesSortProxyModel(ChatPartiesTreeModelPtr sourceModel, QObject *parent /*= nullptr*/)
   : QSortFilterProxyModel(parent)
   , sourceModel_(std::move(sourceModel))
{
   setDynamicSortFilter(true);
   setSourceModel(sourceModel_.get());
}

PartyTreeItem* ChatPartiesSortProxyModel::getInternalData(const QModelIndex& index) const
{
   if (!index.isValid()) {
      return {};
   }

   const auto& sourceIndex = mapToSource(index);
   return static_cast<PartyTreeItem*>(sourceIndex.internalPointer());
}

const std::string& ChatPartiesSortProxyModel::currentUser() const
{
   return sourceModel_->currentUser();
}

Qt::ItemFlags ChatPartiesSortProxyModel::flags(const QModelIndex& index) const
{
   if (!index.isValid()) {
      return Qt::NoItemFlags;
   }

   PartyTreeItem* treeItem = getInternalData(index);
   if (!treeItem) {
      return Qt::NoItemFlags;
   }

   if (UI::ElementType::Container != treeItem->modelType()) {
      return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
   }

   return Qt::NoItemFlags;
}

QModelIndex ChatPartiesSortProxyModel::getProxyIndexById(const std::string& partyId) const
{
   const QModelIndex sourceIndex = sourceModel_->getPartyIndexById(partyId);
   return mapFromSource(sourceIndex);
}

QModelIndex ChatPartiesSortProxyModel::getOTCGlobalRoot() const
{
   const QModelIndex sourceOtcIndex = sourceModel_->getOTCGlobalRoot();
   return mapFromSource(sourceOtcIndex);
}

bool ChatPartiesSortProxyModel::filterAcceptsRow(int row, const QModelIndex& parent) const
{
   Q_ASSERT(sourceModel_);

   auto index = sourceModel_->index(row, 0, parent);
   if (!index.isValid()) {
      return false;
   }

   PartyTreeItem* item = static_cast<PartyTreeItem*>(index.internalPointer());
   if (!item) {
      return false;
   }

   switch (item->modelType()) {
   case UI::ElementType::Party:
      return true;
   case UI::ElementType::Container:
      return item->childCount() > 0;
   default:
      return false;
   }
}

bool ChatPartiesSortProxyModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
   if (!left.isValid() || !right.isValid()) {
      return QSortFilterProxyModel::lessThan(left, right);
   }

   PartyTreeItem* itemLeft = static_cast<PartyTreeItem*>(left.internalPointer());
   PartyTreeItem* itemRight = static_cast<PartyTreeItem*>(right.internalPointer());

   if (!itemLeft || !itemRight) {
      return QSortFilterProxyModel::lessThan(left, right);
   }

   if (itemLeft->modelType() == itemRight->modelType()) {
      if (itemLeft->modelType() == UI::ElementType::Party) {
         Chat::ClientPartyPtr leftParty = itemLeft->data().value<Chat::ClientPartyPtr>();
         Chat::ClientPartyPtr rightParty = itemRight->data().value<Chat::ClientPartyPtr>();
         return leftParty->displayName() < rightParty->displayName();
      }
      else if (itemLeft->modelType() == UI::ElementType::Container) {
         return itemLeft->childNumber() < itemRight->childNumber();
      }
   }

   return QSortFilterProxyModel::lessThan(left, right);
}
