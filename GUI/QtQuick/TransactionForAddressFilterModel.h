/*

***********************************************************************************
* Copyright (C) 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef TRANSACTION_FOR_ADDRESS_FILTER_MODEL_H
#define TRANSACTION_FOR_ADDRESS_FILTER_MODEL_H

#include <QSortFilterProxyModel>

class TransactionForAddressFilterModel : public QSortFilterProxyModel
{
   Q_OBJECT
   Q_PROPERTY(bool positive READ positive WRITE set_positive NOTIFY changed)

public:
   TransactionForAddressFilterModel(QObject *parent = nullptr);

   bool positive() const;
   void set_positive(bool value);

signals:
   void changed();

protected:
   bool filterAcceptsRow(int source_row,
                         const QModelIndex &source_parent) const override;
   bool filterAcceptsColumn(int source_column,
                            const QModelIndex &source_parent) const override;

private:
   bool positive_{false};
};

#endif // TRANSACTION_FOR_ADDRESS_FILTER_MODEL_H
