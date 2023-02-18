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
#include <QDebug>

PendingTransactionFilterModel::PendingTransactionFilterModel(QObject* parent)
   : QSortFilterProxyModel(parent)
{
}

bool PendingTransactionFilterModel::filterAcceptsRow(int source_row,
   const QModelIndex& source_parent) const
{
   const auto confirmationCountIndex = sourceModel()->index(source_row, 5);
   if (sourceModel()->data(confirmationCountIndex, TxListModel::TableRoles::TableDataRole) > 6)
   {
      return true;
   }

   return false;
}

