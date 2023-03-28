/*

***********************************************************************************
* Copyright (C) 2020 - 2022, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "AddressListModel.h"
#include <spdlog/spdlog.h>
#include "ColorScheme.h"
#include "Utils.h"

namespace
{
   static const QHash<int, QByteArray> kRoles{
      {Qt::DisplayRole, "address"},
      {QmlAddressListModel::TableDataRole, "tableData"},
      {QmlAddressListModel::ColorRole, "dataColor"},
      {QmlAddressListModel::AddressTypeRole, "addressType"},
      {QmlAddressListModel::AssetTypeRole, "assetType"} };
}

QmlAddressListModel::QmlAddressListModel(const std::shared_ptr<spdlog::logger>& logger, QObject* parent)
   : QAbstractTableModel(parent), logger_(logger), header_({ tr("Address"), tr("#Tx"), tr("Balance (BTC)"), tr("Comment") })
{}

int QmlAddressListModel::rowCount(const QModelIndex&) const
{
   return table_.size();
}

int QmlAddressListModel::columnCount(const QModelIndex&) const
{
   return header_.size();
}

QVariant QmlAddressListModel::data(const QModelIndex& index, int role) const
{
   const int row = index.row();
   try {
      switch (role)
      {
      case Qt::DisplayRole:
         return table_.at(row).at(0);
      case TableDataRole:
         {
            switch (index.column()) {
            case 0: return table_.at(row).at(0);
            case 1: return QString::number(getTransactionCount(addresses_.at(row).id()));
            case 2: return getAddressBalance(addresses_.at(row).id());
            case 3: return table_.at(row).at(1);
            default: return QString{};
            }
         }
         break;
      case ColorRole: return QColorConstants::White;
      case AddressTypeRole: return table_.at(row).at(2);
      case AssetTypeRole:  return table_.at(row).at(3);
      default: break;
      }
   }
   catch (const std::exception&) {
      return QString{};
   }
   return QVariant();
}

QHash<int, QByteArray> QmlAddressListModel::roleNames() const
{
   return kRoles;
}

void QmlAddressListModel::addRow(const std::string& walletId, const QVector<QString>& row)
{
   if (walletId != expectedWalletId_) {
      logger_->warn("[QmlAddressListModel::addRow] wallet {} not expected ({})", walletId, expectedWalletId_);
      return;
   }
   try {
      addresses_.push_back(bs::Address::fromAddressString(row.at(0).toStdString()));
   }
   catch (const std::exception&) {
      logger_->warn("[{}] {} invalid address {}", __func__, walletId, row.at(0).toStdString());
      addresses_.push_back(bs::Address{});
   }
   QMetaObject::invokeMethod(this, [this, row] {
      beginInsertRows(QModelIndex(), rowCount(), rowCount());
      table_.append(row);
      endInsertRows();
      });
}

void QmlAddressListModel::addRows(const std::string& walletId, const QVector<QVector<QString>>& rows)
{
   if (walletId != expectedWalletId_) {
      logger_->warn("[QmlAddressListModel::addRows] wallet {} not expected ({})", walletId, expectedWalletId_);
      return;
   }
   if (rows.empty()) {
      return;
   }
   for (const auto& row : rows) {
      try {
         const auto& addr = bs::Address::fromAddressString(row.at(0).toStdString());
         bool found = false;
         for (const auto& a : addresses_) {
            if (a == addr) {
               found = true;
               break;
            }
         }
         if (!found) {
            addresses_.push_back(addr);
         }
      }
      catch (const std::exception&) {
         addresses_.push_back(bs::Address{});
      }
   }
   logger_->debug("[{}] {} rows / {} addresses", __func__, rows.size(), addresses_.size());
   QMetaObject::invokeMethod(this, [this, rows] {
      QVector<QVector<QString>> newRows;
      for (const auto& row : rows) {
         bool found = false;
         for (const auto& r : table_) {
            if (r.at(0) == row.at(0)) {
               found = true;
               break;
            }
         }
         if (!found) {
            for (const auto& r : newRows) {
               if (r.at(0) == row.at(0)) {
                  found = true;
                  break;
               }
            }
         }
         if (!found) {
            newRows.append(row);
         }
      }
      bool found = false;
      if (!newRows.empty()) {
         beginInsertRows(QModelIndex(), rowCount(), rowCount() + newRows.size() - 1);
         table_.append(newRows);
         endInsertRows();
      }
      });
}

void QmlAddressListModel::updateRow(const BinaryData& addrPubKey, uint64_t bal, uint32_t nbTx)
{
   pendingBalances_[addrPubKey] = { bal, nbTx };
   for (int i = 0; i < table_.size(); ++i) {
      const auto& addr = addresses_.at(i);
      // logger_->debug("[QmlAddressListModel::updateRow] {} {} {}", addr.display(), bal, nbTx);
      if (addr.id() == addrPubKey) {
         emit dataChanged(createIndex(i, 1), createIndex(i, 2));
         break;
      }
   }
}

void QmlAddressListModel::reset(const std::string& expectedWalletId)
{
   expectedWalletId_ = expectedWalletId;
   beginResetModel();
   addresses_.clear();
   table_.clear();
   endResetModel();
}

QVariant QmlAddressListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
   if (orientation == Qt::Orientation::Horizontal) {
      return header_.at(section);
   }
   return QVariant();
}

quint32 QmlAddressListModel::getTransactionCount(const BinaryData& address) const
{
   if (pendingBalances_.count(address) > 0) {
      return pendingBalances_.at(address).nbTx;
   }
   return 0;
}

QString QmlAddressListModel::getAddressBalance(const BinaryData& address) const
{
   if (pendingBalances_.count(address) > 0) {
      return gui_utils::satoshiToQString(pendingBalances_.at(address).balance);
   }
   return gui_utils::satoshiToQString(0);;
}
