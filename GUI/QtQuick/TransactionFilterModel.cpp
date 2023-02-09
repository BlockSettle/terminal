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

TransactionFilterModel::TransactionFilterModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    connect(this, &TransactionFilterModel::changed, this, &TransactionFilterModel::invalidate);
}

bool TransactionFilterModel::filterAcceptsRow(int source_row,
                                              const QModelIndex &source_parent) const
{
    if (source_row == 0) {
        return true;
    }

    const auto walletNameIndex = sourceModel()->index(source_row, 1);
    const auto transactionTypeIndex = sourceModel()->index(source_row, 2);


    if (!walletName_.isEmpty()) {
        if (sourceModel()->data(walletNameIndex, TxListModel::TableRoles::TableDataRole) != walletName_) {
            return false;
        }
    }
    
    if (!transactionType_.isEmpty()) {
        if (sourceModel()->data(transactionTypeIndex, TxListModel::TableRoles::TableDataRole) != transactionType_) {
            return false;
        }
    }

    return true;
}

const QString &TransactionFilterModel::walletName() const
{
    return walletName_;
}

void TransactionFilterModel::setWalletName(const QString &name)
{
    walletName_ = name;
    emit changed();
}

const QString &TransactionFilterModel::transactionType() const
{
    return transactionType_;
}

void TransactionFilterModel::setTransactionType(const QString &type)
{
    transactionType_ = type;
    emit changed();
}
