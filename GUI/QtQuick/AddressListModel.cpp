/*

***********************************************************************************
* Copyright (C) 2020 - 2022, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "AddressListModel.h"
#include "Address.h"
#include "BTCNumericTypes.h"

namespace {
   static const QHash<int, QByteArray> kRoles{
      {QmlAddressListModel::TableDataRole, "tabledata"},
      {QmlAddressListModel::HeadingRole, "heading"},
      {QmlAddressListModel::FirstColRole, "firstcol"}
   };
}

QmlAddressListModel::QmlAddressListModel(const std::shared_ptr<spdlog::logger>&, QObject* parent)
   : QAbstractTableModel(parent)
{
   table.append({tr("Address"), tr("Balance (BTC)"), tr("#Tx"), tr("Comment")});
}

int QmlAddressListModel::rowCount(const QModelIndex &) const
{
   return table.size();
}

int QmlAddressListModel::columnCount(const QModelIndex &) const
{
   return 4;
}

QVariant QmlAddressListModel::data(const QModelIndex& index, int role) const
{
   if (index.column() > 3) {
      return {};
   }
   switch (role) {
   case TableDataRole:
      return table.at(index.row()).at(index.column());
   case HeadingRole:
      return (index.row() == 0);
   case FirstColRole:
      return (index.column() == 0);
   default: break;
   }
   return QVariant();
}

QHash<int, QByteArray> QmlAddressListModel::roleNames() const
{
   return kRoles;
}

void QmlAddressListModel::addRow(const QVector<QString>& row)
{
   beginInsertRows(QModelIndex(), rowCount(), rowCount());
   try {
      const auto& addr = bs::Address::fromAddressString(row.at(0).toStdString());
      const auto& itAddr = pendingBalances_.find(addr.id());
      if (itAddr != pendingBalances_.end()) {
         auto rowCopy = row;
         rowCopy[1] = QString::number(itAddr->second.balance / BTCNumericTypes::BalanceDivider, 'f', 8);
         rowCopy[2] = QString::number(itAddr->second.nbTx);
         pendingBalances_.erase(itAddr);
         table.append(rowCopy);
      }
      else {
         table.append(row);
      }
   }
   catch (const std::exception&) {
      table.append(row);
   }
   endInsertRows();
}

void QmlAddressListModel::updateRow(const BinaryData& addrPubKey, uint64_t bal, uint32_t nbTx)
{
   bool updated = false;
   for (int i = 1; i < table.size(); ++ i) {
      auto& row = table[i];
      try {
         const auto& addr = bs::Address::fromAddressString(row.at(0).toStdString());
         if (addr.id() == addrPubKey) {
            row[1] = QString::number(bal / BTCNumericTypes::BalanceDivider, 'f', 8);
            row[2] = QString::number(nbTx);
            emit dataChanged(createIndex(i, 1), createIndex(i, 2));
            updated = true;
            break;
         }
      }
      catch (const std::exception&) {}
   }
   if (!updated) {
      pendingBalances_[addrPubKey] = { bal, nbTx };
   }
}

void QmlAddressListModel::clear()
{
   pendingBalances_.clear();
   beginResetModel();
   table.remove(1, table.size() - 1);
   endResetModel();
}
