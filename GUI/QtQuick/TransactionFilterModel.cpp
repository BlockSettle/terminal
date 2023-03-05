/*

***********************************************************************************
* Copyright (C) 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "TransactionFilterModel.h"
#include "TxListModel.h"
#include <QDebug>

TransactionFilterModel::TransactionFilterModel(QObject* parent)
   : QSortFilterProxyModel(parent)
{
   setDynamicSortFilter(true);
   sort(0, Qt::AscendingOrder);
   connect(this, &TransactionFilterModel::changed, this, &TransactionFilterModel::invalidate);
}

bool TransactionFilterModel::filterAcceptsRow(int source_row,
   const QModelIndex& source_parent) const
{
   const auto walletNameIndex = sourceModel()->index(source_row, 1);
   const auto transactionTypeIndex = sourceModel()->index(source_row, 2);

   if (!walletName_.isEmpty())
   {
      if (sourceModel()->data(walletNameIndex, TxListModel::TableRoles::TableDataRole) != walletName_)
      {
         return false;
      }
   }

   if (!transactionType_.isEmpty())
   {
      if (sourceModel()->data(transactionTypeIndex, TxListModel::TableRoles::TableDataRole) != transactionType_)
      {
         return false;
      }
   }

   return true;
}

const QString& TransactionFilterModel::walletName() const
{
   return walletName_;
}

void TransactionFilterModel::setWalletName(const QString& name)
{
   walletName_ = name;
   emit changed();
}

const QString& TransactionFilterModel::transactionType() const
{
   return transactionType_;
}

void TransactionFilterModel::setTransactionType(const QString& type)
{
   transactionType_ = type;
   emit changed();
}

bool TransactionFilterModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
   return sourceModel()->data(sourceModel()->index(left.row(), 5), TxListModel::TableRoles::TableDataRole) < 
      sourceModel()->data(sourceModel()->index(right.row(), 5), TxListModel::TableRoles::TableDataRole);
}
