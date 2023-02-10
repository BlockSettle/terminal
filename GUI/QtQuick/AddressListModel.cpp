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
#include "BTCNumericTypes.h"
#include "ColorScheme.h"

namespace
{
   static const QHash<int, QByteArray> kRoles{
       {QmlAddressListModel::TableDataRole, "tableData"},
       {QmlAddressListModel::HeadingRole, "heading"},
       {QmlAddressListModel::FirstColRole, "firstcol"},
       {QmlAddressListModel::ColorRole, "dataColor"},
       {QmlAddressListModel::AddressTypeRole, "addressType"}};
}

QmlAddressListModel::QmlAddressListModel(const std::shared_ptr<spdlog::logger> &logger, QObject *parent)
    : QAbstractTableModel(parent), logger_(logger), header_({tr("Address"), tr("#Tx"), tr("Balance (BTC)"), tr("Comment")})
{
}

int QmlAddressListModel::rowCount(const QModelIndex &) const
{
   return table_.size() + 1;
}

int QmlAddressListModel::columnCount(const QModelIndex &) const
{
   return header_.size();
}

QVariant QmlAddressListModel::data(const QModelIndex &index, int role) const
{
   try
   {
      switch (role)
      {
      case TableDataRole:
         if (index.row() == 0) {
            return header_.at(index.column());
         }
         else
         {
            const int row = index.row() - 1;
            switch (index.column()) {
            case 0: return table_.at(row).at(0);
            case 1: return QString::number(pendingBalances_.at(addresses_.at(row).id()).nbTx);
            case 2: return QString::number(pendingBalances_.at(addresses_.at(row).id()).balance / BTCNumericTypes::BalanceDivider, 'f', 8);
            case 3: return table_.at(row).at(1);
            default: return QString{};
            }
         }
         break;
      case HeadingRole: return (index.row() == 0);
      case FirstColRole: return (index.column() == 0);
      case ColorRole:
         if (index.row() == 0) {
            return ColorScheme::tableHeaderColor;
         }
         else {
            return QColorConstants::White;
         }
      case AddressTypeRole: return table_.at(index.row() - 1).at(2);
      default: break;
      }
   }
   catch (const std::exception &)
   {
      return QString{};
   }
   return QVariant();
}

QHash<int, QByteArray> QmlAddressListModel::roleNames() const
{
   return kRoles;
}

void QmlAddressListModel::addRow(const std::string &walletId, const QVector<QString> &row)
{
   if (walletId != expectedWalletId_)
   {
      logger_->warn("[QmlAddressListModel::addRow] wallet {} not expected ({})", walletId, expectedWalletId_);
      return;
   }
   try
   {
      addresses_.push_back(bs::Address::fromAddressString(row.at(0).toStdString()));
   }
   catch (const std::exception &)
   {
      addresses_.push_back(bs::Address{});
   }
   beginInsertRows(QModelIndex(), rowCount(), rowCount());
   table_.append(row);
   endInsertRows();
}

void QmlAddressListModel::addRows(const std::string &walletId, const QVector<QVector<QString>> &rows)
{
   if (walletId != expectedWalletId_)
   {
      logger_->warn("[QmlAddressListModel::addRows] wallet {} not expected ({})", walletId, expectedWalletId_);
      return;
   }
   if (rows.empty())
   {
      return;
   }
   for (const auto &row : rows)
   {
      try
      {
         addresses_.push_back(bs::Address::fromAddressString(row.at(0).toStdString()));
      }
      catch (const std::exception &)
      {
         addresses_.push_back(bs::Address{});
      }
   }
   beginInsertRows(QModelIndex(), rowCount(), rowCount() + rows.size() - 1);
   table_.append(rows);
   endInsertRows();
}

void QmlAddressListModel::updateRow(const BinaryData &addrPubKey, uint64_t bal, uint32_t nbTx)
{
   pendingBalances_[addrPubKey] = {bal, nbTx};
   for (int i = 0; i < table_.size(); ++i)
   {
      const auto &addr = addresses_.at(i);
      // logger_->debug("[QmlAddressListModel::updateRow] {} {} {}", addr.display(), bal, nbTx);
      if (addr.id() == addrPubKey)
      {
         emit dataChanged(createIndex(i + 1, 1), createIndex(i + 1, 2));
         break;
      }
   }
}

void QmlAddressListModel::reset(const std::string &expectedWalletId)
{
   expectedWalletId_ = expectedWalletId;
   beginResetModel();
   addresses_.clear();
   table_.clear();
   endResetModel();
}
