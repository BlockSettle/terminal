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
#include "ColorScheme.h"

namespace {
   static const QHash<int, QByteArray> kRoles{
      {TxInputsModel::TableDataRole, "tableData"},
      {TxInputsModel::HeadingRole, "heading"},
      {TxInputsModel::ColorRole, "dataColor"},
      {TxInputsModel::SelectedRole, "selected"},
      {TxInputsModel::ExpandedRole, "expanded"},
      {TxInputsModel::CanBeExpandedRole, "is_expandable"}
   };
}

TxInputsModel::TxInputsModel(const std::shared_ptr<spdlog::logger>& logger
   , TxOutputsModel* outs, QObject* parent)
   : QAbstractTableModel(parent), logger_(logger), outsModel_(outs)
   , header_{{ColumnAddress, tr("Address/Hash")}, {ColumnTx, tr("#Tx")},
            {ColumnComment, tr("Comment")}, {ColumnBalance, tr("Balance (BTC)")}}
{
}

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
   case SelectedRole:
      return (index.column() == 0) ? (selection_.find(index.row() - 1) != selection_.end()) : false;
   case ExpandedRole:
      return (index.row() > 0 && index.column() == 0) ? data_[index.row() - 1].expanded : false;
   case CanBeExpandedRole:
      return (index.row() > 0 && index.column() == 0) ? data_[index.row() - 1].txId.empty() : false;
   case ColorRole:
      return dataColor(index.row(), index.column());
   default: break;
   }
   return QVariant();
}

QColor TxInputsModel::dataColor(int row, int col) const
{
   if (row == 0) {
      return ColorScheme::tableHeaderColor;
   }
   return ColorScheme::tableTextColor;
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
   emit rowCountChanged();
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
            emit rowCountChanged();
         }
         else {
            if (data_.at(addrIndex).expanded) {
               beginInsertRows(QModelIndex(), addrIndex + 2, addrIndex + 2);
               data_.insert(data_.cbegin() + addrIndex + 1, { {}, utxo.getTxHash(), utxo.getTxOutIndex() });
               endInsertRows();
               emit rowCountChanged();
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
      }
      for (const auto& idx : selChanges) {
         if (idx.second >= 0) {
            selection_.insert(idx.second);
         }
      }
   };

   std::map<int, int> selChanges;
   if (entry.expanded) {
      entry.expanded = false;
      for (const auto& sel : selection_) {
         if (sel > row + 1) {
            if (sel <= (row + 1 + it->second.size())) {
               selChanges[sel] = -1;
            }
            else {
               selChanges[sel] = sel - it->second.size();
            }
         }
      }
      beginRemoveRows(QModelIndex(), row + 2, row + it->second.size() + 1);
      changeSelection(selChanges);
      data_.erase(data_.cbegin() + row + 1, data_.cbegin() + row + it->second.size() + 1);
      endRemoveRows();

      emit rowCountChanged();
   }
   else {
      entry.expanded = true;

      bool isSelectedParent = (selection_.find(row + 1) != selection_.end());
      for (int i=0; i<it->second.size(); i++) {
         bool isSelectedChild = (selection_.find(row + 2 + i) != selection_.end());
         if (isSelectedParent || isSelectedChild) {
            selChanges[-1] = row + 2 + i;
         }
      }

      for (const auto& sel : selection_) {
         if (sel > row + 1) {
            selChanges[sel] = sel + it->second.size();
         }
      }

      std::vector<Entry> entries;
      for (const auto& utxo : it->second) {
         entries.push_back({ {}, utxo.getTxHash(), utxo.getTxOutIndex()});
      }
      beginInsertRows(QModelIndex(), row + 2, row + it->second.size() + 1);
      data_.insert(data_.cbegin() + row + 1, entries.cbegin(), entries.cend());
      changeSelection(selChanges);
      endInsertRows();
      emit rowCountChanged();
      emit dataChanged(createIndex(row + 1, 0), createIndex(rowCount() - 1, 0), { SelectedRole });
   }
}

void TxInputsModel::toggleSelection(int row)
{
   if (row == 0) {
      auto temp_selection = selection_;
      for (const auto & sel_row : temp_selection) {
         if (sel_row != 0) {
            const auto& entry = data_.at(sel_row - 1);
            if (entry.txId.empty()) {
               toggleSelection(sel_row);
            }
         }
      }
      temp_selection = selection_;
      for (const auto & sel_row : temp_selection) {
         if (sel_row != 0) {
            toggleSelection(sel_row);
         }
      }
      if (selection_.find(0) == selection_.end()) {
         selection_.insert(0);
         for (int i_row = 1;  i_row < rowCount(); i_row ++) {
            const auto& entry = data_.at(i_row-1);
            if (entry.txId.empty()) {
               toggleSelection(i_row);
            }
         }
      }
      else {
         selection_.erase(0);
      }
      return;
   }

   --row;
//   if (data_.at(row).expanded) {
//      return;
//   }
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
   int selStart = row + 1;
   int selEnd = row + 1;
   if (selection_.find(row + 1) == selection_.end()) {
      selection_.insert(row + 1);
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
            if (data_.at(row).expanded) {
               for (int i=0; i<itAddr->second.size(); i++) {
                   selection_.insert(row + 2 + i);
               }
               selEnd = row + 1 + itAddr->second.size();
            }
         }
      }
   }
   else {
      selection_.erase(row+1);
      if (!entry.txId.empty()) {
         nbTx_--;
         selectedBalance_ -= utxo.getValue();
         int rowParent = row - 1;
         while (rowParent >= 0) {
            if (data_.at(rowParent).expanded) {
               selection_.erase(rowParent + 1);
               break;
            }
            rowParent--;
         }
         if (rowParent >= 0)
         selStart = rowParent + 1;
      }
      else {
         if (itAddr != utxos_.end()) {
            nbTx_ -= itAddr->second.size();
            for (const auto& utxo : itAddr->second) {
               selectedBalance_ -= utxo.getValue();
            }
         }
         if (data_.at(row).expanded) {
            for (int i=0; i<itAddr->second.size(); i++) {
               selection_.erase(row + 2 + i);
            }
            selEnd = row + 1 + itAddr->second.size();
         }
      }
   }
   emit selectionChanged();
   emit dataChanged(createIndex(selStart, 0), createIndex(selEnd, 0), {SelectedRole});
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
      for (int idx : selection_) {
         --idx;
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
      return header_[col];
   }
   const auto& entry = data_.at(row - 1);
   const auto& itUTXOs = entry.address.empty() ? utxos_.end() : utxos_.find(entry.address);
   switch (col) {
   case ColumnAddress:
      if (!entry.txId.empty()) {
         const auto& txId = entry.txId.toHexStr(true);
         std::string str = txId;
         if (txId.size() > 40)
            str = txId.substr(0, 20) + "..." + txId.substr(txId.size() - 21, 20);

         return QString::fromStdString(str);
      }
      else {
         return QString::fromStdString(entry.address.display());
      }
   case ColumnTx:
      if (itUTXOs != utxos_.end()) {
         return QString::number(itUTXOs->second.size());
      }
      break;
   case ColumnBalance:
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
