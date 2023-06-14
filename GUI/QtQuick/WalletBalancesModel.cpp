/*

***********************************************************************************
* Copyright (C) 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "WalletBalancesModel.h"
#include <fstream>
#include <QDateTime>
#include <spdlog/spdlog.h>
#include "StringUtils.h"
#include "Utils.h"

namespace {
   static const QHash<int, QByteArray> kWalletBalanceRoles{
      {WalletBalance::NameRole, "name"},
      {WalletBalance::IdRole, "id"},
      {WalletBalance::TotalRole, "total"},
      {WalletBalance::ConfirmedRole, "confirmed"},
      {WalletBalance::UnconfirmedRole, "unconfirmed"},
      {WalletBalance::NbAddrRole, "nb_used_addrs"}
   };
}

WalletBalancesModel::WalletBalancesModel(const std::shared_ptr<spdlog::logger>& logger, QObject* parent)
   : QAbstractTableModel(parent), logger_(logger)
{}

int WalletBalancesModel::rowCount(const QModelIndex &) const
{
   return wallets_.size();
}

QVariant WalletBalancesModel::data(const QModelIndex& index, int role) const
{
   if (index.row() < 0) {
      return {};
   }
   FieldFunc ff{ nullptr };
   switch (role) {
   case WalletBalance::NameRole:
      return QString::fromStdString(wallets_.at(index.row()).walletName);
   case WalletBalance::IdRole:
      return QString::fromStdString(wallets_.at(index.row()).walletId);
   case WalletBalance::TotalRole:
      ff = [](const Balance& bal) { return gui_utils::xbtToQString(bal.total); };
      break;
   case WalletBalance::ConfirmedRole:
      ff = [](const Balance& bal) { return gui_utils::xbtToQString(bal.confirmed); };
      break;
   case WalletBalance::UnconfirmedRole:
      ff = [](const Balance& bal) { return gui_utils::xbtToQString(bal.unconfirmed); };
      break;
   case WalletBalance::NbAddrRole:
      ff = [](const Balance& bal) { return QString::number(bal.nbAddresses); };
      break;
   default: break;
   }
   if (ff != nullptr) {
      return getBalance(wallets_.at(index.row()).walletId, ff);
   }
   return QVariant();
}

QString WalletBalancesModel::getBalance(const std::string& walletId
   , const FieldFunc& ff, const QString defaultValue) const
{
   const auto& itBal = balances_.find(walletId);
   if (itBal == balances_.end()) {
      return defaultValue;
   }
   return ff(itBal->second);
}

QHash<int, QByteArray> WalletBalancesModel::roleNames() const
{
   return kWalletBalanceRoles;
}

void WalletBalancesModel::addWallet(const Wallet& wallet)
{
   const auto idx = getWalletIndex(wallet.walletId);
   if (idx >= 0) {
      logger_->warn("[{}] wallet {} already exists (#{})", __func__, wallet.walletId, idx);
      return;
   }
   //logger_->debug("[WalletBalancesModel::addWallet] adding #{}: {}", rowCount(), wallet.walletName);
   beginInsertRows(QModelIndex(), rowCount(), rowCount());
   wallets_.push_back(wallet);
   endInsertRows();

   //find index of new created wallet
   if (!createdWalletId_.empty() && wallet.walletId == createdWalletId_) {
      createdWalletId_.clear();
      int findIndex = wallets_.size() - 1;
      emit walletSelected(findIndex);
   }
   emit rowCountChanged();
   emit changed();
}

int WalletBalancesModel::getWalletIndex(const std::string& walletId) const
{
   int idx = -1;
   for (int i = 0; i < wallets_.size(); ++i) {
      if (wallets_.at(i).walletId == walletId) {
         idx = i;
         break;
      }
   }
   return idx;
}

void WalletBalancesModel::deleteWallet(const std::string& walletId)
{
   const auto idx = getWalletIndex(walletId);
   if (idx < 0) {
      logger_->warn("[{}] wallet {} is not in the list", __func__, walletId);
      return;
   }
   QMetaObject::invokeMethod(this, [this, idx] {
      beginRemoveRows(QModelIndex(), idx, idx);
      wallets_.erase(wallets_.cbegin() + idx);
      endRemoveRows();
      emit rowCountChanged();
      emit changed();
      if (idx >= wallets_.size())
          emit walletSelected(wallets_.size() - 1);
   });
}

void WalletBalancesModel::rename(const std::string& walletId, const std::string& newName)
{
   const auto idx = getWalletIndex(walletId);
   if (idx < 0) {
      logger_->warn("[{}] wallet {} is not in the list", __func__, walletId);
      return;
   }
   wallets_.at(idx).walletName = newName;
   emit dataChanged(createIndex(idx, 0), createIndex(idx, 0), { WalletBalance::NameRole });
}

QStringList WalletBalancesModel::walletNames() const
{
   QStringList result;
   for (const auto& w : wallets_) {
      result.append(QString::fromStdString(w.walletName));
   }
   return result;
}

void WalletBalancesModel::clear()
{
   beginResetModel();
   wallets_.clear();
   balances_.clear();
   endResetModel();

   emit changed();
   emit rowCountChanged();
}

void WalletBalancesModel::setWalletBalance(const std::string& walletId, const Balance& bal)
{
   balances_[walletId] = bal;
   const int row = getWalletIndex(walletId);
   if (row >= 0) {
      //logger_->debug("[{}] {} {} found at row {}", __func__, txDet.txHash.toHexStr(), txDet.hdWalletId, row);
      emit dataChanged(createIndex(row, 0), createIndex(row, 0), { WalletBalance::TotalRole
         , WalletBalance::ConfirmedRole, WalletBalance::UnconfirmedRole, WalletBalance::NbAddrRole });
   }
   else {
      logger_->warn("[{}] {} not found", __func__, walletId);
   }
   emit changed();
}

void WalletBalancesModel::setSelectedWallet(int index)
{
   selectedWallet_ = index;
   emit changed();
}

int WalletBalancesModel::selectedWallet() const
{
   return selectedWallet_;
}

QString WalletBalancesModel::confirmedBalance() const
{
   if (selectedWallet_ >= 0 && selectedWallet_ < wallets_.size()) {
      return getBalance(wallets_.at(selectedWallet_).walletId
         , [](const Balance& bal) { return gui_utils::xbtToQString(bal.confirmed); });
   }
   return tr("-");
}

QString WalletBalancesModel::unconfirmedBalance() const
{
   if (selectedWallet_ >= 0 && selectedWallet_ < wallets_.size()) {
      return getBalance(wallets_.at(selectedWallet_).walletId
         , [](const Balance& bal) { return gui_utils::xbtToQString(bal.unconfirmed); });
   }
   return tr("-");
}

QString WalletBalancesModel::totalBalance() const
{
   if (selectedWallet_ >= 0 && selectedWallet_ < wallets_.size()) {
      return getBalance(wallets_.at(selectedWallet_).walletId
         , [](const Balance& bal) { return gui_utils::xbtToQString(bal.total); });
   }
   return tr("-");
}

QString WalletBalancesModel::numberAddresses() const
{
   if (selectedWallet_ >= 0 && selectedWallet_ < wallets_.size()) {
      return getBalance(wallets_.at(selectedWallet_).walletId
         , [](const Balance& bal) { return QString::number(bal.nbAddresses); }, QString::fromLatin1("0"));
   }
   return tr("-");
}

bool WalletBalancesModel::nameExist(const std::string& walletName)
{
   for (int i = 0; i < wallets_.size(); ++i) {
      const auto& w = wallets_.at(i);
      if (w.walletName == walletName) {
         return true;
      }
   }
   return false;
}

void WalletBalancesModel::setCreatedWalletId(const std::string& walletId)
{
   createdWalletId_ = walletId;
}
