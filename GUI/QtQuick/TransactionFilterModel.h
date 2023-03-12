/*

***********************************************************************************
* Copyright (C) 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef TRANSACTION_FILTER_MODEL_H
#define TRANSACTION_FILTER_MODEL_H

#include <QSortFilterProxyModel>
#include "SettingsController.h"

class TransactionFilterModel: public QSortFilterProxyModel
{
   Q_OBJECT
   Q_PROPERTY(QString walletName READ walletName WRITE setWalletName NOTIFY changed)
   Q_PROPERTY(QString transactionType READ transactionType WRITE setTransactionType NOTIFY changed)

public:
   TransactionFilterModel(std::shared_ptr<SettingsController> settings);

   const QString& walletName() const;
   void setWalletName(const QString&);
   const QString& transactionType() const;
   void setTransactionType(const QString&);

signals:
   void changed();

protected:
   bool filterAcceptsRow(int source_row,
      const QModelIndex& source_parent) const override;
   bool lessThan(const QModelIndex & left, const QModelIndex & right) const override;

private:
   QString walletName_;
   QString transactionType_;
   std::shared_ptr<SettingsController> settings_;
};

#endif // TRANSACTION_FILTER_MODEL_H
