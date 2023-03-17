/*

***********************************************************************************
* Copyright (C) 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "PendingTransactionFilterModel.h"
#include "TxListModel.h"

PendingTransactionFilterModel::PendingTransactionFilterModel(QObject* parent)
   : QSortFilterProxyModel(parent)
{
   setDynamicSortFilter(true);
   sort(0, Qt::AscendingOrder);
}

bool PendingTransactionFilterModel::filterAcceptsRow(int source_row,
   const QModelIndex& source_parent) const
{
   const auto confirmationCountIndex = sourceModel()->index(source_row, 5);
   if (sourceModel()->data(confirmationCountIndex, TxListModel::TableRoles::TableDataRole) >= 6)
   {
      return false;
   }

   return true;
}

bool PendingTransactionFilterModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
   return sourceModel()->data(sourceModel()->index(left.row(), 5), TxListModel::TableRoles::TableDataRole) < 
      sourceModel()->data(sourceModel()->index(right.row(), 5), TxListModel::TableRoles::TableDataRole);
}
