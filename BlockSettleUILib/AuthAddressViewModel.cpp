/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include <QFont>
#include <QTreeView>
#include "AuthAddressViewModel.h"
#include "EncryptionUtils.h"


AuthAddressViewModel::AuthAddressViewModel(const std::shared_ptr<AuthAddressManager>& authManager, QObject *parent)
   : QAbstractItemModel(parent)
   , authManager_(authManager)
   , defaultAddr_(authManager->getDefault())
{
   connect(authManager_.get(), &AuthAddressManager::AddressListUpdated, this, &AuthAddressViewModel::onAddressListUpdated, Qt::QueuedConnection);
}

int AuthAddressViewModel::columnCount(const QModelIndex&) const
{
   return static_cast<int>(AuthAddressViewColumns::ColumnsCount);
}

int AuthAddressViewModel::rowCount(const QModelIndex&) const
{
   return authManager_->GetAddressCount();
}

QVariant AuthAddressViewModel::data(const QModelIndex &index, int role) const
{
   const auto address = getAddress(index);

   if (role == Qt::DisplayRole) {
      switch(static_cast<AuthAddressViewColumns>(index.column())) {
      case AuthAddressViewColumns::ColumnName:
         return QString::fromStdString(address.display());
      case AuthAddressViewColumns::ColumnState:
         switch (authManager_->GetState(address)) {
         case AddressVerificationState::VerificationFailed:
            return tr("State loading failed");
         case AddressVerificationState::InProgress:
            return tr("Loading state");
         case AddressVerificationState::NotSubmitted:
            return tr("Not Submitted");
         case AddressVerificationState::Submitted:
            return tr("Submitted");
         case AddressVerificationState::PendingVerification:
            return tr("Pending verification");
         case AddressVerificationState::VerificationSubmitted:
            return tr("Verification submitted");
         case AddressVerificationState::Verified:
            return tr("Verified");
         case AddressVerificationState::Revoked:
            return tr("Revoked");
         case AddressVerificationState::RevokedByBS:
            return tr("Revoked by BS");
         }
      default:
         return QVariant();
      }
   }
   else if (role == Qt::FontRole) {
      if (address.prefixed() == defaultAddr_.prefixed()) {
         QFont font;
         font.setBold(true);
         return font;
      }
   }

   return QVariant();
}

QVariant AuthAddressViewModel::headerData(int section, Qt::Orientation orientation, int role) const
{
   if (orientation != Qt::Horizontal) {
      return QVariant();
   }

   if (role == Qt::DisplayRole) {
      switch(static_cast<AuthAddressViewColumns>(section)) {
      case AuthAddressViewColumns::ColumnName:
         return tr("Address");
      case AuthAddressViewColumns::ColumnState:
         return tr("Status");
      default:
         return QVariant();
      }
   }

   return QVariant();
}

QModelIndex AuthAddressViewModel::index(int row, int column, const QModelIndex&) const
{
   if ((row < 0) || (row >= rowCount()) || (column < 0) || (column >= columnCount())) {
      return QModelIndex();
   }

   return createIndex(row, column, row);
}

QModelIndex AuthAddressViewModel::parent(const QModelIndex&) const
{
   return QModelIndex();
}

bs::Address AuthAddressViewModel::getAddress(const QModelIndex& index) const
{
   return authManager_->GetAddress(index.row());
}

void AuthAddressViewModel::setDefaultAddr(const bs::Address &addr)
{
   defaultAddr_ = addr;
   onAddressListUpdated();
}

void AuthAddressViewModel::onAddressListUpdated()
{
   const auto treeView = qobject_cast<QTreeView *>(QObject::parent());
   QModelIndexList selRows;
   if (treeView && treeView->selectionModel()) {
      selRows = treeView->selectionModel()->selectedRows();
   }
   emit beginResetModel();
   emit endResetModel();
   if (!selRows.empty()) {
      treeView->selectionModel()->select(selRows[0], QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
   }
}
