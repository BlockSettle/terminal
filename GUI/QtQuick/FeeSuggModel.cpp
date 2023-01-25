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

#include <QDebug>

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
{
   //connect(this, &FeeSuggestionModel::rowsInserted, this, &FeeSuggestionModel::rowCountChanged);
   //connect(this, &FeeSuggestionModel::rowsRemoved, this, &FeeSuggestionModel::rowCountChanged);
}

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
      return tr("%1 blocks (%2): %3 s/b").arg(data_.at(index.row()).nbBlocks)
         .arg(data_.at(index.row()).estTime).arg(QString::number(data_.at(index.row()).satoshis, 'f', 1));
   case BlocksRole:
      return data_.at(index.row()).nbBlocks;
   case TimeRole:
      return data_.at(index.row()).estTime;
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

std::map<uint32_t, QString> FeeSuggestionModel::feeLevels()
{
   return {
         { 2, tr("20 minutes")},
         { 4, tr("40 minutes")},
         { 6, tr("60 minutes")},
         { 12, tr("2 hours")},
         { 24, tr("4 hours")},
         { 48, tr("8 hours")},
         { 144, tr("24 hours")},
         { 504, tr("3 days")},
         { 1008, tr("7 days")}
   };
}

void FeeSuggestionModel::addRows(const std::map<uint32_t, float>& feeLevels)
{
   if (feeLevels.empty()) {
      return;
   }
   const auto& levelMapping = FeeSuggestionModel::feeLevels();
   decltype(data_) newRows;
   for (const auto& feeLevel : feeLevels) {
      QString estTime;
      const auto& itLevel = levelMapping.find(feeLevel.first);
      if (itLevel == levelMapping.end()) {
         estTime = tr("%1 minutes").arg(feeLevel.first * 10);
      }
      else {
         estTime = itLevel->second;
      }
      FeeSuggestion row{ feeLevel.first, std::move(estTime), feeLevel.second };
      newRows.emplace_back(std::move(row));
   }
   beginInsertRows(QModelIndex(), rowCount(), rowCount() + newRows.size() - 1);
   data_.insert(data_.cend(), newRows.begin(), newRows.end());
   endInsertRows();
   emit rowCountChanged();
}

void FeeSuggestionModel::clear()
{
   beginResetModel();
   data_.clear();
   endResetModel();
   emit rowCountChanged();
}
