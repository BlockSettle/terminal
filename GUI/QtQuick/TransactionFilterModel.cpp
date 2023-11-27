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

TransactionFilterModel::TransactionFilterModel(std::shared_ptr<SettingsController> settings)
   : QSortFilterProxyModel()
   , settings_(settings)
{
   setDynamicSortFilter(true);
   sort(0, Qt::AscendingOrder);
   connect(this, &TransactionFilterModel::changed, this, &TransactionFilterModel::invalidate);

   if (settings_ != nullptr) {
      connect(settings_.get(), &SettingsController::reset, this, [this]()
      {
         if (settings_->hasParam(ApplicationSettings::Setting::TransactionFilterWalletName)) {
            walletName_ = settings_->getParam(ApplicationSettings::Setting::TransactionFilterWalletName).toString();
         }
         if (settings_->hasParam(ApplicationSettings::Setting::TransactionFilterTransactionType)) {
            transactionType_ = settings_->getParam(ApplicationSettings::Setting::TransactionFilterTransactionType).toString();
         }
         emit changed();
      });
   }
}

bool TransactionFilterModel::filterAcceptsRow(int source_row,
   const QModelIndex& source_parent) const
{
   const auto walletNameIndex = sourceModel()->index(source_row, 1);
   const auto transactionTypeIndex = sourceModel()->index(source_row, 2);

   if (!walletName_.isEmpty()) {
      if (sourceModel()->data(walletNameIndex, TxListModel::TableRoles::TableDataRole)
         != walletName_) {
         return false;
      }
   }

   if (!transactionType_.isEmpty()) {
      if (sourceModel()->data(transactionTypeIndex, TxListModel::TableRoles::TableDataRole)
         != transactionType_) {
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
   if (walletName_ != name) {
      walletName_ = name;
      settings_->setParam(ApplicationSettings::Setting::TransactionFilterWalletName, walletName_);
      emit changed();
   }
}

const QString& TransactionFilterModel::transactionType() const
{
   return transactionType_;
}

void TransactionFilterModel::setTransactionType(const QString& type)
{
   if (transactionType_ != type) {
      transactionType_ = type;
      settings_->setParam(ApplicationSettings::Setting::TransactionFilterTransactionType
         , transactionType_);
      emit changed();
   }
}

bool TransactionFilterModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
   return sourceModel()->data(sourceModel()->index(left.row(), 5), TxListModel::TableRoles::TableDataRole) < 
      sourceModel()->data(sourceModel()->index(right.row(), 5), TxListModel::TableRoles::TableDataRole);
}
