/*

***********************************************************************************
* Copyright (C) 2020 - 2022, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "TxInputsModel.h"
#include <spdlog/spdlog.h>
#include "Address.h"
#include "BTCNumericTypes.h"
#include "CoinSelection.h"
#include "TxOutputsModel.h"

namespace {
   static const QHash<int, QByteArray> kRoles{
      {TxInputsModel::TableDataRole, "tableData"},
      {TxInputsModel::HeadingRole, "heading"},
      {TxInputsModel::WidthRole, "colWidth"},
      {TxInputsModel::ColorRole, "dataColor"},
      {TxInputsModel::BgColorRole, "bgColor"},
   };
}

TxInputsModel::TxInputsModel(const std::shared_ptr<spdlog::logger>& logger
   , TxOutputsModel* outs, QObject* parent)
   : QAbstractTableModel(parent), logger_(logger), outsModel_(outs)
   , header_{ {}, tr("Address/Hash"), tr("#Tx"), tr("Balance (BTC)"), tr("Comment") }
{ }

int TxInputsModel::rowCount(const QModelIndex &) const
{
   return data_.size() + 1;
}

int TxInputsModel::columnCount(const QModelIndex &) const
{
   return header_.size();
}

QVariant TxInputsModel::data(const QModelIndex& index, int role) const
{
   switch (role) {
   case TableDataRole:
      return getData(index.row(), index.column());
   case HeadingRole:
      return (index.row() == 0);
   case WidthRole:
      return colWidth(index.column());
   case ColorRole:
      return dataColor(index.row(), index.column());
   case BgColorRole:
      return bgColor(index.row());
   default: break;
   }
   return QVariant();
}

QHash<int, QByteArray> TxInputsModel::roleNames() const
{
   return kRoles;
}

void TxInputsModel::clear()
{
   beginResetModel();
   utxos_.clear();
   data_.clear();
   selection_.clear();
   preSelected_.clear();
   endResetModel();
}

void TxInputsModel::addUTXOs(const std::vector<UTXO>& utxos)
{
   for (const auto& utxo : utxos) {
      try {
         const auto& addr = bs::Address::fromUTXO(utxo);
         utxos_[addr].push_back(utxo);
         int addrIndex = -1;
         for (int i = 0; i < data_.size(); ++i) {
            if (data_.at(i).address == addr) {
               addrIndex = i;
               break;
            }
         }
         if (addrIndex < 0) {
            beginInsertRows(QModelIndex(), rowCount(), rowCount());
            data_.push_back({ addr });
            endInsertRows();
         }
         else {
            if (data_.at(addrIndex).expanded) {
               beginInsertRows(QModelIndex(), addrIndex + 2, addrIndex + 2);
               data_.insert(data_.cbegin() + addrIndex + 1, { {}, utxo.getTxHash(), utxo.getTxOutIndex() });
               endInsertRows();
            }
         }
      }
      catch (const std::exception&) {
         continue;
      }
   }
   if (collectUTXOsForAmount_ > 0) {
      collectUTXOsFor(collectUTXOsForAmount_);
      collectUTXOsForAmount_ = 0;
   }
}

void TxInputsModel::toggle(int row)
{
   --row;
   auto& entry = data_[row];
   if (!entry.txId.empty()) {
      return;
   }
   const auto& it = utxos_.find(entry.address);
   if (it == utxos_.end()) {
      return;
   }
   const auto& changeSelection = [this](const std::map<int, int>& selChanges)
   {
      for (const auto& idx : selChanges) {
         selection_.erase(idx.first);
         if (idx.second >= 0) {
            selection_.insert(idx.second);
         }
      }
   };
   std::map<int, int> selChanges;
   if (entry.expanded) {
      entry.expanded = false;
      for (const auto& sel : selection_) {
         if (sel > row) {
            if (sel <= (row + it->second.size())) {
               selChanges[sel] = -1;
            }
            else {
               selChanges[sel] = sel - it->second.size();
            }
         }
      }
      beginRemoveRows(QModelIndex(), row + 2, row + it->second.size() + 1);
      changeSelection(selChanges);
      data_.erase(data_.cbegin() + row + 1, data_.cbegin() + row + it->second.size());
      endRemoveRows();
   }
   else {
      entry.expanded = true;
      for (const auto& sel : selection_) {
         if (sel == row) {
            selChanges[sel] = -1;
         }
         else if (sel > row) {
            selChanges[sel] = sel + it->second.size();
         }
      }
      std::vector<Entry> entries;
      for (const auto& utxo : it->second) {
         entries.push_back({ {}, utxo.getTxHash(), utxo.getTxOutIndex()});
      }
      beginInsertRows(QModelIndex(), row + 2, row + it->second.size() + 1);
      changeSelection(selChanges);
      data_.insert(data_.cbegin() + row + 1, entries.cbegin(), entries.cend());
      endInsertRows();
      emit dataChanged(createIndex(row + 1, 0), createIndex(row + 1, columnCount() - 1), { BgColorRole });
   }
}

void TxInputsModel::toggleSelection(int row)
{
   --row;
   if (data_.at(row).expanded) {
      return;
   }
   const auto& entry = data_.at(row);
   const auto& itAddr = utxos_.find(entry.address);
   UTXO utxo{};
   if (itAddr == utxos_.end()) {
      bool found = false;
      for (const auto& byAddr : utxos_) {
         for (const auto& u : byAddr.second) {
            if ((u.getTxHash() == entry.txId) && (u.getTxOutIndex() == entry.txOutIndex)) {
               utxo = u;
               found = true;
               break;
            }
         }
         if (found) {
            break;
         }
      }
   }
   if (selection_.find(row) == selection_.end()) {
      selection_.insert(row);
      if (!entry.txId.empty()) {
         nbTx_++;
         selectedBalance_ += utxo.getValue();
      }
      else {
         if (itAddr != utxos_.end()) {
            nbTx_ += itAddr->second.size();
            for (const auto& utxo : itAddr->second) {
               selectedBalance_ += utxo.getValue();
            }
         }
      }
   }
   else {
      selection_.erase(row);
      if (!entry.txId.empty()) {
         nbTx_--;
         selectedBalance_ -= utxo.getValue();
      }
      else {
         if (itAddr != utxos_.end()) {
            nbTx_ -= itAddr->second.size();
            for (const auto& utxo : itAddr->second) {
               selectedBalance_ -= utxo.getValue();
            }
         }
      }
   }
   emit selectionChanged();
   emit dataChanged(createIndex(row + 1, 0), createIndex(row + 1, columnCount() - 1));
}

QUTXOList* TxInputsModel::getSelection()
{
   QList<QUTXO*> result;
   const double amount = outsModel_ ? outsModel_->totalAmount() : 0;
   logger_->debug("[{}] total amount: {}", __func__, amount);
   if (amount > 0) { // auto selection
      const auto& it = preSelected_.find((int)std::floor(amount * BTCNumericTypes::BalanceDivider));
      if (it != preSelected_.end()) {
         return new QUTXOList(it->second, (QObject*)this);
      }
      if (utxos_.empty()) {
         collectUTXOsForAmount_ = amount;
         return nullptr;
      }
      result = collectUTXOsFor(amount);
      preSelected_[(int)std::floor(amount * BTCNumericTypes::BalanceDivider)] = result;
   }
   else {
      for (const int idx : selection_) {
         const auto& entry = data_.at(idx);
         if (!entry.txId.empty()) {
            bool added = false;
            for (const auto& byAddr : utxos_) {
               for (const auto& utxo : byAddr.second) {
                  if ((entry.txId == utxo.getTxHash()) && (entry.txOutIndex == utxo.getTxOutIndex())) {
                     result.push_back(new QUTXO(utxo, (QObject*)this));
                     added = true;
                     break;
                  }
               }
               if (added) {
                  break;
               }
            }
         }
         else {
            selectedBalance_ = 0;
            const auto& itUTXO = utxos_.find(entry.address);
            if (itUTXO != utxos_.end()) {
               for (const auto& utxo : itUTXO->second) {
                  selectedBalance_ += utxo.getValue();
                  result.push_back(new QUTXO(utxo, (QObject*)this));
               }
            }
            nbTx_ = result.size();
            preSelected_[0] = result;
         }
         emit selectionChanged();
      }
   }
   return new QUTXOList(result, (QObject*)this);
}

QList<QUTXO*> TxInputsModel::collectUTXOsFor(double amount)
{
   QList<QUTXO*> result;
   std::vector<UTXO> allUTXOs;
   for (const auto& byAddr : utxos_) {
      allUTXOs.insert(allUTXOs.cend(), byAddr.second.cbegin(), byAddr.second.cend());
   }
   bs::Address::decorateUTXOs(allUTXOs);
   const auto& recipients = outsModel_->recipients();
   std::map<unsigned, std::vector<std::shared_ptr<Armory::Signer::ScriptRecipient>>> recipientsMap;
   for (unsigned i = 0; i < recipients.size(); ++i) {
      recipientsMap[i] = { recipients.at(i) };
   }
   float feePerByte = fee_.isEmpty() ? 1.0 : std::stof(fee_.toStdString());
   auto payment = Armory::CoinSelection::PaymentStruct(recipientsMap, 0, feePerByte, 0);
   Armory::CoinSelection::CoinSelection coinSelection([allUTXOs](uint64_t) { return allUTXOs; }
   , std::vector<AddressBookEntry>{}, UINT64_MAX, topBlock_);
   const auto selection = coinSelection.getUtxoSelectionForRecipients(payment, allUTXOs);

   selectedBalance_ = 0;
   for (const auto& utxo : selection.utxoVec_) {
      selectedBalance_ += utxo.getValue();
      result.push_back(new QUTXO(utxo, (QObject*)this));
   }
   nbTx_ = result.size();
   emit selectionChanged();
   return result;
}

QVariant TxInputsModel::getData(int row, int col) const
{
   if (row == 0) {
      return header_.at(col);
   }
   const auto& entry = data_.at(row - 1);
   const auto& itUTXOs = entry.address.empty() ? utxos_.end() : utxos_.find(entry.address);
   switch (col) {
   case 0:
      return (selection_.find(row -1) != selection_.end());
   case 1:
      if (!entry.txId.empty()) {
         const auto& txId = entry.txId.toHexStr(true);
         std::string str = "   " + txId.substr(0, 8) + "..." + txId.substr(txId.size() - 9, 8);
         return QString::fromStdString(str);
      }
      else {
         return QString::fromStdString(entry.address.display());
      }
   case 2:
      if (itUTXOs != utxos_.end()) {
         return QString::number(itUTXOs->second.size());
      }
      break;
   case 3:
      if (itUTXOs != utxos_.end()) {
         uint64_t balance = 0;
         for (const auto& utxo : itUTXOs->second) {
            balance += utxo.getValue();
         }
         return QString::number(balance / BTCNumericTypes::BalanceDivider, 'f', 8);
      }
      else {
         for (const auto& byAddr : utxos_) {
            for (const auto& utxo : byAddr.second) {
               if ((entry.txId == utxo.getTxHash()) && (entry.txOutIndex == utxo.getTxOutIndex())) {
                  return QString::number(utxo.getValue() / BTCNumericTypes::BalanceDivider, 'f', 8);
               }
            }
         }
      }
      break;
   default: break;
   }
   return {};
}

QColor TxInputsModel::dataColor(int row, int col) const
{
   if (row == 0) {
      return QColorConstants::DarkGray;
   }
   return QColorConstants::LightGray;
}

QColor TxInputsModel::bgColor(int row) const
{
   if (row == 0) {
      return QColorConstants::Black;
   }
   if (selection_.find(row - 1) != selection_.end()) {
      return QColorConstants::DarkCyan;
   }
   return QColorConstants::DarkBlue;
}

float TxInputsModel::colWidth(int col) const
{  // width ratio, sum should give columnCount() as a result
   switch (col) {
   case 0:  return 0.2;
   case 1:  return 2.6;
   case 2:  return 0.4;
   case 3:  return 0.8;
   case 4:  return 1;
   default: break;
   }
   return 1.0;
}
