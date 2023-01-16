/*

***********************************************************************************
* Copyright (C) 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "FeeSuggModel.h"
#include "Address.h"
#include "BTCNumericTypes.h"

namespace {
   static const QHash<int, QByteArray> kRoles{
      {FeeSuggestionModel::TextRole, "text"},
      {FeeSuggestionModel::BlocksRole, "nb_blocks"},
      {FeeSuggestionModel::TimeRole, "time"},
      {FeeSuggestionModel::ValueRole, "value"}
   };
}

FeeSuggestionModel::FeeSuggestionModel(const std::shared_ptr<spdlog::logger>& logger, QObject* parent)
   : QAbstractTableModel(parent), logger_(logger)
{}

int FeeSuggestionModel::rowCount(const QModelIndex &) const
{
   return data_.size();
}

int FeeSuggestionModel::columnCount(const QModelIndex &) const
{
   return 1;
}

QVariant FeeSuggestionModel::data(const QModelIndex& index, int role) const
{
   switch (role) {
   case TextRole:
      return tr("%1 blocks (%2 minutes): %3 s/b").arg(data_.at(index.row()).nbBlocks)
         .arg(data_.at(index.row()).minutes).arg(data_.at(index.row()).satoshis);
   case BlocksRole:
      return data_.at(index.row()).nbBlocks;
   case TimeRole:
      return data_.at(index.row()).minutes;
   case ValueRole:
      return data_.at(index.row()).satoshis;
   default: break;
   }
   return QVariant();
}

QHash<int, QByteArray> FeeSuggestionModel::roleNames() const
{
   return kRoles;
}

void FeeSuggestionModel::addRow(const FeeSuggestion& row)
{
   beginInsertRows(QModelIndex(), rowCount(), rowCount());
   data_.push_back(row);
   endInsertRows();
}

void FeeSuggestionModel::clear()
{
   beginResetModel();
   data_.clear();
   endResetModel();
}
