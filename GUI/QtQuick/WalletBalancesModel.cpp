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
{
}

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
      logger_->debug("[WalletBalancesModel::data] name: {}", wallets_.at(index.row()).walletName);
      return QString::fromStdString(wallets_.at(index.row()).walletName);
   case WalletBalance::IdRole:
      return QString::fromStdString(wallets_.at(index.row()).walletId);
   case WalletBalance::TotalRole:
      ff = [](const Balance& bal) { return QString::number(bal.total, 'f', 8); };
      break;
   case WalletBalance::ConfirmedRole:
      ff = [](const Balance& bal) { return QString::number(bal.confirmed, 'f', 8); };
      break;
   case WalletBalance::UnconfirmedRole:
      ff = [](const Balance& bal) { return QString::number(bal.unconfirmed, 'f', 8); };
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
   , const FieldFunc& ff) const
{
   const auto& itBal = balances_.find(walletId);
   if (itBal == balances_.end()) {
      return tr("-");
   }
   return ff(itBal->second);
}

QHash<int, QByteArray> WalletBalancesModel::roleNames() const
{
   return kWalletBalanceRoles;
}

void WalletBalancesModel::addWallet(const const Wallet& wallet)
{
   for (int i = 0; i < wallets_.size(); ++i) {
      const auto& w = wallets_.at(i);
      if (wallet.walletId == w.walletId) {
         return;
      }
   }
   //logger_->debug("[WalletBalancesModel::addWallet] adding #{}: {}", rowCount(), wallet.walletName);
   beginInsertRows(QModelIndex(), rowCount(), rowCount());
   wallets_.push_back(wallet);
   endInsertRows();

   emit rowCountChanged();
}

QStringList WalletBalancesModel::wallets() const
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

   emit rowCountChanged();
}

void WalletBalancesModel::setWalletBalance(const std::string& walletId, const Balance& bal)
{
   balances_[walletId] = bal;
   int row = -1;
   for (int i = 0; i < wallets_.size(); ++i) {
      const auto& w = wallets_.at(i);
      if (w.walletId == walletId) {
         row = i;
         break;
      }
   }
   if (row >= 0) {
      //logger_->debug("[{}] {} {} found at row {}", __func__, txDet.txHash.toHexStr(), txDet.hdWalletId, row);
      emit dataChanged(createIndex(row, 0), createIndex(row, 0), { WalletBalance::TotalRole
         , WalletBalance::ConfirmedRole, WalletBalance::UnconfirmedRole, WalletBalance::NbAddrRole });
   }
   else {
      logger_->warn("[{}] {} not found", __func__, walletId);
   }
}
