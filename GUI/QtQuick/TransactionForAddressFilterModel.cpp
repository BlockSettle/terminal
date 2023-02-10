/*

***********************************************************************************
* Copyright (C) 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "TransactionForAddressFilterModel.h"
#include "TxListModel.h"
#include <QDebug>

TransactionForAddressFilterModel::TransactionForAddressFilterModel(QObject *parent)
   : QSortFilterProxyModel(parent)
{
   connect(this, &TransactionForAddressFilterModel::changed, this, &TransactionForAddressFilterModel::invalidate);
}

bool TransactionForAddressFilterModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
   if (source_row == 0) {
      return true;
   }

   const auto index = sourceModel()->index(source_row, 5);
   const auto transaction_value = sourceModel()->data(index, TxListForAddr::TableRoles::TableDataRole);
   if ((positive_ && transaction_value < 0) || (!positive_ && transaction_value > 0)) {
      return false;
   }

   return true;
}

bool TransactionForAddressFilterModel::filterAcceptsColumn(int source_column, const QModelIndex &source_parent) const
{
   if (source_column == 0 || source_column == 1 || source_column == 2 || source_column == 5) {
      return true;
   }
   return false;
}

bool TransactionForAddressFilterModel::positive() const
{
   return positive_;
}

void TransactionForAddressFilterModel::set_positive(bool value)
{
   positive_ = value;
   emit changed();
}
