/*

***********************************************************************************
* Copyright (C) 2020 - 2022, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "TxListModel.h"
#include <spdlog/spdlog.h>

namespace {
   static const QHash<int, QByteArray> kRoles{
      {TxListModel::TableDataRole, "tabledata"},
      {TxListModel::HeadingRole, "heading"}
   };
}

TxListModel::TxListModel(const std::shared_ptr<spdlog::logger>&, QObject* parent)
   : QAbstractTableModel(parent)
{
   table.append({ tr("Date"), tr("Wallet"), tr("Type"), tr("Address"), tr("Amount")
      , tr("Confirmations"), tr("Flag"), tr("Comment") });
}

int TxListModel::rowCount(const QModelIndex &) const
{
   return table.size();
}

int TxListModel::columnCount(const QModelIndex &) const
{
   return 8;
}

QVariant TxListModel::data(const QModelIndex& index, int role) const
{
   if (index.column() > 7) {
      return {};
   }
   switch (role) {
   case TableDataRole:
      return table.at(index.row()).at(index.column());
   case HeadingRole:
      return (index.row() == 0);
   default: break;
   }
   return QVariant();
}

QHash<int, QByteArray> TxListModel::roleNames() const
{
   return kRoles;
}

void TxListModel::addRow(const QVector<QString>& row)
{
   beginInsertRows(QModelIndex(), rowCount(), rowCount());
   table.append(row);
   endInsertRows();
}

void TxListModel::clear()
{
   beginResetModel();
   table.remove(1, table.size() - 1);
   endResetModel();
}
