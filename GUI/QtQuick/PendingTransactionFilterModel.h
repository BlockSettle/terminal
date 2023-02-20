/*

***********************************************************************************
* Copyright (C) 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef PENDING_TRANSACTION_FILTER_MODEL_H
#define PENDING_TRANSACTION_FILTER_MODEL_H

#include <QSortFilterProxyModel>

class PendingTransactionFilterModel: public QSortFilterProxyModel
{
   Q_OBJECT

public:
   PendingTransactionFilterModel(QObject* parent = nullptr);

protected:
   bool filterAcceptsRow(int source_row,
      const QModelIndex& source_parent) const override;
};

#endif // PENDING_TRANSACTION_FILTER_MODEL_H
