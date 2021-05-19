/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
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
   connect(authManager_.get(), &AuthAddressManager::AuthWalletChanged, this, &AuthAddressViewModel::onAddressListUpdated, Qt::QueuedConnection);
}

AuthAddressViewModel::AuthAddressViewModel(QObject* parent)
   : QAbstractItemModel(parent)
{}

AuthAddressViewModel::~AuthAddressViewModel() noexcept = default;

int AuthAddressViewModel::columnCount(const QModelIndex&) const
{
   return static_cast<int>(AuthAddressViewColumns::ColumnsCount);
}

int AuthAddressViewModel::rowCount(const QModelIndex&) const
{
   return addresses_.size();
}

QVariant AuthAddressViewModel::data(const QModelIndex &index, int role) const
{
   if (!index.isValid() || index.row() < 0 || index.row() >= addresses_.size()) {
      return {};
   }

   const auto address = addresses_[index.row()];

   if (role == Qt::DisplayRole) {
      switch(static_cast<AuthAddressViewColumns>(index.column())) {
      case AuthAddressViewColumns::ColumnName:
         return QString::fromStdString(address.display());
      case AuthAddressViewColumns::ColumnState:
         if (authManager_) {
            switch (authManager_->GetState(address)) {
            case AuthAddressManager::AuthAddressState::Unknown:
               return tr("Loading state...");
            case AuthAddressManager::AuthAddressState::NotSubmitted:
               return tr("Not Submitted");
            case AuthAddressManager::AuthAddressState::Submitted:
               return tr("Submitted");
            case AuthAddressManager::AuthAddressState::Tainted:
               return tr("Not Submitted");
            case AuthAddressManager::AuthAddressState::Verifying:
               return tr("Verification pending");
            case AuthAddressManager::AuthAddressState::Verified:
               return tr("Verified");
            case AuthAddressManager::AuthAddressState::Revoked:
               return tr("Revoked");
            case AuthAddressManager::AuthAddressState::RevokedByBS:
               return tr("Invalidated by BS");
            case AuthAddressManager::AuthAddressState::Invalid:
               return tr("State loading failed");
            }
         }
         else {
            const auto& itState = states_.find(address);
            if (itState == states_.end()) {
               return tr("Loading state...");
            }
            switch (itState->second) {
            case AddressVerificationState::Verified:
               return tr("Verified");
            case AddressVerificationState::Revoked:
               return tr("Revoked");
            case AddressVerificationState::Virgin:
               if (submittedAddresses_.find(address) != submittedAddresses_.end()) {
                  return tr("Submitted");
               }
               else {
                  return tr("Not submitted");
               }
            case AddressVerificationState::Verifying:
               return tr("Verifying");
            default: return tr("Unknown %1").arg((int)itState->second);
            }
         }
      default:
         return {};
      }
   }
   else if (role == Qt::FontRole) {
      if (!defaultAddr_.empty() && (address.prefixed() == defaultAddr_.prefixed())) {
         QFont font;
         font.setBold(true);
         return font;
      }
   }

   return {};
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

   return createIndex(row, column);
}

QModelIndex AuthAddressViewModel::parent(const QModelIndex&) const
{
   return {};
}

bs::Address AuthAddressViewModel::getAddress(const QModelIndex& index) const
{
   if (!index.isValid() || index.row() < 0 || index.row() >= addresses_.size()) {
      return {};
   }

   return addresses_[index.row()];
}

bool AuthAddressViewModel::isAddressNotSubmitted(int row) const
{
   if (row < 0 || row >= addresses_.size()) {
      return false;
   }

   const auto& address = addresses_[row];
   if (authManager_) {
      const auto addrState = authManager_->GetState(address);
      return addrState == AuthAddressManager::AuthAddressState::NotSubmitted ||
         addrState == AuthAddressManager::AuthAddressState::Tainted;
   }
   else {
      return (submittedAddresses_.find(address) == submittedAddresses_.end());
   }
}

void AuthAddressViewModel::setDefaultAddr(const bs::Address &addr)
{
   defaultAddr_ = addr;
   for (int i = 0; i < addresses_.size(); ++i) {
      if (addresses_[i].prefixed() == defaultAddr_.prefixed()) {
         emit dataChanged(index(i, 0), index(i, 0), { Qt::FontRole });
         return;
      }
   }
}

void AuthAddressViewModel::onAuthAddresses(const std::vector<bs::Address>&addrs
   , const std::map<bs::Address, AddressVerificationState>& states)
{
   if (!addrs.empty()) {
      beginResetModel();
      addresses_ = addrs;
      endResetModel();
   }
   if (!states.empty()) {
      for (const auto& state : states) {
         states_[state.first] = state.second;
      }
      emit dataChanged(index(0, 0), index(addresses_.size() - 1, 0));
   }
}

void AuthAddressViewModel::onSubmittedAuthAddresses(const std::vector<bs::Address>& addrs)
{
   submittedAddresses_.insert(addrs.cbegin(), addrs.cend());
}

bool AuthAddressViewModel::canSubmit(const bs::Address& addr) const
{
   const auto& itState = states_.find(addr);
   if ((itState == states_.end()) || (itState->second != AddressVerificationState::Virgin)) {
      return false;
   }
   return (submittedAddresses_.find(addr) == submittedAddresses_.end());
}

AddressVerificationState AuthAddressViewModel::getState(const bs::Address& addr) const
{
   try {
      return states_.at(addr);
   }
   catch (const std::exception&) {
      return AddressVerificationState::VerificationFailed;
   }
}

void AuthAddressViewModel::onAddressListUpdated()
{
   // store selection
   const auto treeView = qobject_cast<QTreeView *>(QObject::parent());
   std::pair<int, std::string> selectedRowToName;
   if (treeView && treeView->selectionModel() && treeView->selectionModel()->hasSelection()) {
      selectedRowToName.first = treeView->selectionModel()->selectedRows()[0].row();
      selectedRowToName.second = getAddress(index(selectedRowToName.first,
         static_cast<int>(AuthAddressViewColumns::ColumnName))).display();
   }

   // do actual update
   const int sizeBeforeReset = addresses_.size();
   emit beginResetModel();
   addresses_.clear();
   const int total = authManager_->GetAddressCount();
   addresses_.reserve(total);
   for (int i = 0; i < total; ++i) {
      addresses_.push_back(authManager_->GetAddress(i));
   }
   emit endResetModel();

   // restore selection if needed
   if (sizeBeforeReset >= addresses_.size()
      && selectedRowToName.first < addresses_.size()
      && selectedRowToName.second == addresses_[selectedRowToName.first].display()) {
      emit updateSelectionAfterReset(selectedRowToName.first);
   }
}

AuthAdressControlProxyModel::AuthAdressControlProxyModel(AuthAddressViewModel *sourceModel, QWidget *parent)
   : QSortFilterProxyModel(parent)
   , sourceModel_(sourceModel)
{
   setDynamicSortFilter(true);
   setSourceModel(sourceModel_);
}

AuthAdressControlProxyModel::~AuthAdressControlProxyModel() = default;

void AuthAdressControlProxyModel::setVisibleRowsCount(int rows)
{
   visibleRowsCount_ = rows;
   invalidate();
}

void AuthAdressControlProxyModel::increaseVisibleRowsCountByOne()
{
   ++visibleRowsCount_;
   invalidate();
}

int AuthAdressControlProxyModel::getVisibleRowsCount() const
{
   return visibleRowsCount_;
}

bs::Address AuthAdressControlProxyModel::getAddress(const QModelIndex& index) const
{
   if (!index.isValid()) {
      return {};
   }

   const auto& sourceIndex = mapToSource(index);
   return sourceModel_->getAddress(sourceIndex);
}

bool AuthAdressControlProxyModel::isEmpty() const
{
   return rowCount() == 0;
}

QModelIndex AuthAdressControlProxyModel::getFirstUnsubmitted() const
{
   if (isEmpty()) {
      return {};
   }

   for (int i = 0; i < rowCount(); ++i) {
      if (sourceModel_->isAddressNotSubmitted(i)) {
         return index(i, 0);
      }
   }

   return {};
}

bool AuthAdressControlProxyModel::isUnsubmittedAddressVisible() const
{
   if (isEmpty()) {
      return false;
   }

   for (int i = 0; i < visibleRowsCount_; ++i) {
      if (sourceModel_->isAddressNotSubmitted(i)) {
         return true;
      }
   }

   return false;
}

bool AuthAdressControlProxyModel::filterAcceptsRow(int row, const QModelIndex&) const
{
   return visibleRowsCount_ > row;
}
