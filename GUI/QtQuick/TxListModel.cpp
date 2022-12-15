/*

***********************************************************************************
* Copyright (C) 2020 - 2022, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "TxListModel.h"
#include <QDateTime>
#include <spdlog/spdlog.h>
#include "StringUtils.h"

namespace {
   static const QHash<int, QByteArray> kRoles{
      {TxListModel::TableDataRole, "tableData"},
      {TxListModel::HeadingRole, "heading"},
      {TxListModel::ColorRole, "dataColor"},
      {TxListModel::WidthRole, "colWidth"},
   };
}

TxListModel::TxListModel(const std::shared_ptr<spdlog::logger>& logger, QObject* parent)
   : QAbstractTableModel(parent), logger_(logger)
   , header_{ tr("Date"), tr("Wallet"), tr("Type"), tr("Address"), tr("Amount")
      , tr("#Conf"), tr("Flag"), tr("Comment") }
{}

int TxListModel::rowCount(const QModelIndex &) const
{
   return data_.size() + 1;
}

int TxListModel::columnCount(const QModelIndex &) const
{
   return header_.size();
}

QString TxListModel::getData(int row, int col) const
{
   if (row > data_.size()) {
      return {};
   }
   if (row == 0) {
      return header_.at(col);
   }
   const auto& entry = data_.at(row - 1);
   switch (col) {
   case 0:  return QDateTime::fromSecsSinceEpoch(entry.txTime).toString();
   case 1:  return walletNameById(*entry.walletIds.cbegin());
   case 2:  return txType(row - 1);
   case 3: {
      bs::Address address;
      if (!entry.addresses.empty()) {
         address = *entry.addresses.cbegin();
      }
      return QString::fromStdString(address.display());
   }
   case 4: return QString::number(std::abs(entry.value) / BTCNumericTypes::BalanceDivider, 'f', 8);
   case 5: return QString::number(entry.nbConf);
   case 6: return txFlag(row - 1);
   case 7: {
      const auto& itComm = txComments_.find(entry.txHash.toBinStr());
      if (itComm != txComments_.end()) {
         return QString::fromStdString(itComm->second);
      }
      break;
   }
   default: break;
   }
   return {};
}

QColor TxListModel::dataColor(int row, int col) const
{
   if (row == 0) {
      return QColorConstants::DarkGray;
   }
   const auto& entry = data_.at(row - 1);
   if (col == 5) {
      switch (entry.nbConf) {
      case 0:  return QColorConstants::Red;
      case 1:
      case 2:
      case 3:
      case 4:
      case 5:  return QColorConstants::Yellow;
      default: return QColorConstants::Green;
      }
   }
   else if (col == 2) {
      const auto& itTxDet = txDetails_.find(row - 1);
      if (itTxDet != txDetails_.end()) {
         switch (itTxDet->second.direction) {
         case bs::sync::Transaction::Direction::Received:   return QColorConstants::Green;
         case bs::sync::Transaction::Direction::Sent:       return QColorConstants::Red;
         case bs::sync::Transaction::Direction::Internal:   return QColorConstants::Cyan;
         default: break;
         }
      }
      return QColorConstants::DarkGray;
   }
   else if (col == 4) {
      if (entry.value < 0) {
         return qRgb(208, 192, 192);
      }
      return qRgb(192, 208, 192);
   }
   else if (col == 6) {
      return QColorConstants::DarkGray;
   }
   return QColorConstants::LightGray;
}

float TxListModel::colWidth(int col) const
{  // width ratio, sum should give columnCount() as a result
   switch (col) {
   case 0:  return 1.5;
   case 1:  return 0.6;
   case 2:  return 0.8;
   case 3:  return 2.0;
   case 4:  return 0.8;
   case 5:  return 0.5;
   case 6:  return 0.5;
   case 7:  return 1.0;
   }
   return 1.0;
}

QString TxListModel::walletNameById(const std::string& walletId) const
{
   auto itName = walletNames_.find(walletId);
   if (itName != walletNames_.end()) {
      return QString::fromStdString(itName->second);
   }
   itName = walletNames_.find(bs::toLower(walletId));
   if (itName != walletNames_.end()) {
      return QString::fromStdString(itName->second);
   }
   return QString::fromStdString(walletId);
}

QString TxListModel::txType(int row) const
{
   const auto& itTxDet = txDetails_.find(row);
   if (itTxDet != txDetails_.end()) {
      switch (itTxDet->second.direction) {
      case bs::sync::Transaction::Direction::Received:   return tr("Received");
      case bs::sync::Transaction::Direction::Sent:       return tr("Sent");
      case bs::sync::Transaction::Direction::Internal:   return tr("Internal");
      case bs::sync::Transaction::Direction::Unknown:    return tr("Unknown");
      default: return QString::number((int)itTxDet->second.direction);
      }
   }
   return {};
}

QString TxListModel::txFlag(int row) const
{
   const auto& itTxDet = txDetails_.find(row);
   if (itTxDet != txDetails_.end()) {
      if (itTxDet->second.tx.isRBF()) {
         return tr("RBF");
      }
   }
   return {};
}

QVariant TxListModel::data(const QModelIndex& index, int role) const
{
   if (index.column() >= header_.size()) {
      return {};
   }
   switch (role) {
   case TableDataRole:
      return getData(index.row(), index.column());
   case HeadingRole:
      return (index.row() == 0);
   case ColorRole:
      return dataColor(index.row(), index.column());
   case WidthRole:
      return colWidth(index.column());
   default: break;
   }
   return QVariant();
}

QHash<int, QByteArray> TxListModel::roleNames() const
{
   return kRoles;
}

void TxListModel::addRows(const std::vector<bs::TXEntry>& entries)
{
   beginInsertRows(QModelIndex(), rowCount(), rowCount() + entries.size() - 1);
   data_.insert(data_.end(), entries.cbegin(), entries.cend());
   endInsertRows();
}

void TxListModel::prependRow(const bs::TXEntry& entry)
{
   beginInsertRows(QModelIndex(), 1, 1);
   data_.insert(data_.cbegin(), entry);
   endInsertRows();
}

void TxListModel::addRow(const bs::TXEntry& entry)
{
   beginInsertRows(QModelIndex(), rowCount(), rowCount());
   data_.push_back(entry);
   endInsertRows();
}

void TxListModel::clear()
{
   beginResetModel();
   data_.clear();
   txDetails_.clear();
   endResetModel();
}

void TxListModel::setTxComment(const std::string& txHash, const std::string& comment)
{
   txComments_[txHash] = comment;
   int rowFirst = 0, rowLast = 0;
   for (int i = 0; i < data_.size(); ++i) {
      const auto& txEntryHash = data_.at(i).txHash.toBinStr();
      if (txHash == txEntryHash) {
         if (!rowFirst) {
            rowFirst = i + 1;
         }
         rowLast = i + 1;
      }
   }
   if (rowFirst && rowLast) {
      emit dataChanged(createIndex(rowFirst, 7), createIndex(rowLast, 7));
   }
}

void TxListModel::setWalletName(const std::string& walletId, const std::string& walletName)
{
   const auto& [it, inserted] = walletNames_.insert({ walletId, walletName });
   if (inserted) {
      int rowFirst = 0, rowLast = 0;
      for (int i = 0; i < data_.size(); ++i) {
         const auto& entry = data_.at(i);
         if (entry.walletIds.empty()) {
            continue;
         }
         const auto& entryWalletId = bs::toLower(*entry.walletIds.cbegin());
         if (bs::toLower(walletId) == entryWalletId) {
            if (!rowFirst) {
               rowFirst = i + 1;
            }
            rowLast = i + 1;
         }
      }
      if (rowFirst && rowLast) {
         emit dataChanged(createIndex(rowFirst, 1), createIndex(rowLast, 1));
      }
      //logger_->debug("[{}] found rows {}-{} for {} = {}", __func__, rowFirst, rowLast, walletId, walletName);
   }
}

void TxListModel::setDetails(const bs::sync::TXWalletDetails& txDet)
{
   int row = 0;
   for (int i = 0; i < data_.size(); ++i) {
      const auto& entry = data_.at(i);
      if (entry.txHash == txDet.txHash) {
         for (const auto& walletId : entry.walletIds) {
            if (bs::toLower(walletId) == bs::toLower(txDet.walletId)) {
               txDetails_[i] = txDet;
               row = i + 1;
               break;
            }
         }
         if (row) {
            break;
         }
      }
   }
   if (row) {
      emit dataChanged(createIndex(row, 2), createIndex(row, 6));
   }
   else {
      //logger_->debug("[{}] {} {} not found", __func__, txDet.txHash.toHexStr(), txDet.walletId);
   }
}

void TxListModel::setCurrentBlock(uint32_t nbBlock)
{
   if (!nbBlock) {
      return;
   }
   if (!curBlock_) {
      curBlock_ = nbBlock;
      return;
   }
   const int diff = nbBlock - curBlock_;
   curBlock_ = nbBlock;
   for (auto& entry : data_) {
      entry.nbConf += diff;
   }
   emit dataChanged(createIndex(1, 5), createIndex(data_.size(), 5));
}
