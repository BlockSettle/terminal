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
#include "Utils.h"

namespace {
   static const QHash<int, QByteArray> kRoles{
      {TxInputsModel::TableDataRole, "tableData"},
      {TxInputsModel::HeadingRole, "heading"},
      {TxInputsModel::ColorRole, "dataColor"},
      {TxInputsModel::SelectedRole, "selected"},
      {TxInputsModel::ExpandedRole, "expanded"},
      {TxInputsModel::CanBeExpandedRole, "is_expandable"},
      {TxInputsModel::EditableRole, "is_editable"}
   };
}

TxInputsModel::TxInputsModel(const std::shared_ptr<spdlog::logger>& logger
   , TxOutputsModel* outs, QObject* parent)
   : QAbstractTableModel(parent), logger_(logger), outsModel_(outs)
   , header_{{ColumnAddress, tr("Address/Hash")}, {ColumnTx, tr("#Tx")},
            {ColumnComment, tr("Comment")}, {ColumnBalance, tr("Balance (BTC)")}}
{}

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
      if (index.column() != 0) {
          return false;
      }
      else if (index.row() == 0) {
          return selectionRoot_;
      }
      else if (data_[index.row() - 1].txId.empty()) {
         return (selectionAddresses_.find(data_[index.row() - 1].address)
                 != selectionAddresses_.end());
      }
      else {
         return (selectionUtxos_.find({data_[index.row() - 1].txId, data_[index.row() - 1].txOutIndex})
                 != selectionUtxos_.end());
      }
   case ExpandedRole:
      return (index.row() > 0 && index.column() == 0) ? data_[index.row() - 1].expanded : false;
   case CanBeExpandedRole:
      return (index.row() > 0 && index.column() == 0) ? data_[index.row() - 1].txId.empty() : false;
   case ColorRole:
      return dataColor(index.row(), index.column());
   case EditableRole: 
      return isInputSelectable(index.row());
   default: break;
   }
   return QVariant();
}

QVariant TxInputsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
   if (orientation == Qt::Orientation::Horizontal) {
      return header_[section];
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
   decltype(fixedEntries_) fixedEntries;
   fixedEntries.swap(fixedEntries_);
   beginResetModel();
   utxos_.clear();
   data_.clear();
   selectionRoot_ = false;
   preSelected_.clear();
   endResetModel();
   setFixedInputs(fixedEntries);
   clearSelection();
   emit rowCountChanged();
}

void TxInputsModel::addUTXOs(const std::vector<UTXO>& utxos)
{
   QMetaObject::invokeMethod(this, [this, utxos] {
      for (const auto& utxo : utxos) {
         try {
            const auto& addr = bs::Address::fromUTXO(utxo);

            if (std::find(utxos_[addr].begin(), utxos_[addr].end(), utxo) != utxos_[addr].end()) {
                continue;
            }

            utxos_[addr].push_back(utxo);
            int addrIndex = -1;
            for (int i = fixedEntries_.size(); i < data_.size(); ++i) {
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
      if (collectUTXOsForAmount_) {
         collectUTXOsFor();
         collectUTXOsForAmount_ = false;
      }
   });
}

int TxInputsModel::setFixedUTXOs(const std::vector<UTXO>& utxos)
{
   fixedUTXOs_ = utxos;
   nbTx_ = 0;
   selectedBalance_ = 0;
   selectionUtxos_.clear();
   for (const auto& utxo : utxos) {
      for (const auto& entry : fixedEntries_) {
         if ((entry.txId == utxo.getTxHash()) && (entry.txOutIndex == utxo.getTxOutIndex())) {
            logger_->debug("[{}] UTXO selection: {}@{}", __func__, utxo.getTxHash().toHexStr(true), utxo.getTxOutIndex());
            selectionUtxos_.insert({ utxo.getTxHash(), utxo.getTxOutIndex() });
            selectedBalance_ += utxo.getValue();
            nbTx_++;
            const auto& addr = bs::Address::fromUTXO(utxo);
            utxos_[addr].push_back(utxo);
            break;
         }
      }
   }
   emit selectionChanged();
   emit dataChanged(createIndex(0, 0), createIndex(fixedEntries_.size() - 1, 0), {SelectedRole});
   logger_->debug("[{}] nbTX: {}, balance: {}", __func__, nbTx_, selectedBalance_);
   return nbTx_;
}

void TxInputsModel::addEntries(const std::vector<Entry>& entries)
{
   if (entries.empty()) {
      return;
   }
   beginInsertRows(QModelIndex(), rowCount(), rowCount() + entries.size() - 1);
   data_.insert(data_.cend(), entries.cbegin(), entries.cend());
   endInsertRows();
   emit rowCountChanged();
}

void TxInputsModel::setFixedInputs(const std::vector<Entry>& entries)
{
   if (entries.empty()) {
      return;
   }
   logger_->debug("[{}] {} entries", __func__, entries.size());
   if (!fixedEntries_.empty() && (data_.size() >= fixedEntries_.size())) {
      logger_->debug("[{}] removing {} fixed entries", __func__, fixedEntries_.size());
      beginRemoveRows(QModelIndex(), 0, fixedEntries_.size() - 1);
      data_.erase(data_.cbegin(), data_.cbegin() + fixedEntries_.size());
      endRemoveRows();
   }
   std::vector<Entry> treeEntries;
   for (const auto& entry : entries) {
      auto it = std::find_if(treeEntries.cbegin(), treeEntries.cend(), [entry](const Entry& e)
         { return e.address == entry.address; });
      if (it == treeEntries.end()) {
         auto addrEntry = entry;
         addrEntry.txId.clear();
         addrEntry.expanded = true;
         treeEntries.push_back(addrEntry);
      }
      auto txEntry = entry;
      txEntry.address.clear();
      treeEntries.push_back(txEntry);
   }
   fixedEntries_ = treeEntries;
   beginInsertRows(QModelIndex(), 0, treeEntries.size() - 1);
   data_.insert(data_.cbegin(), treeEntries.cbegin(), treeEntries.cend());
   endInsertRows();
   emit rowCountChanged();
}

void TxInputsModel::toggle(int row)
{
   --row;
   if (row < fixedEntries_.size()) {
      return;
   }
   auto& entry = data_[row];
   if (!entry.txId.empty()) {
      return;
   }
   const auto& it = utxos_.find(entry.address);
   if (it == utxos_.end()) {
      return;
   }

   std::map<int, int> selChanges;
   if (entry.expanded) {
      entry.expanded = false;
      beginRemoveRows(QModelIndex(), row + 2, row + it->second.size() + 1);
      data_.erase(data_.cbegin() + row + 1, data_.cbegin() + row + it->second.size() + 1);
      endRemoveRows();

      emit rowCountChanged();
   }
   else {
      entry.expanded = true;
      std::vector<Entry> entries;
      for (const auto& utxo : it->second) {
         entries.push_back({ {}, utxo.getTxHash(), utxo.getTxOutIndex()});
      }
      beginInsertRows(QModelIndex(), row + 2, row + it->second.size() + 1);
      data_.insert(data_.cbegin() + row + 1, entries.cbegin(), entries.cend());
      endInsertRows();
      emit rowCountChanged();
      emit dataChanged(createIndex(row + 1, 0), createIndex(rowCount() - 1, 0), { SelectedRole });
   }
}

void TxInputsModel::toggleSelection(int row)
{
   if (row == 0) {
      selectionRoot_ = !selectionRoot_;

      if (!selectionRoot_) {
         clearSelection();
      }
      else {
         for (int iRow = fixedEntries_.size() + 1;  iRow < rowCount(); iRow ++) {
            const auto& entry = data_.at(iRow-1);
            if (entry.txId.empty()) {
               selectionAddresses_.insert(entry.address);
            }
            const auto& itAddr = utxos_.find(entry.address);
            if (itAddr != utxos_.end()) {
                for (const auto& u : itAddr->second) {
                   selectionUtxos_.insert({u.getTxHash(), u.getTxOutIndex()});
                   selectedBalance_ += u.getValue();
                   nbTx_++;
                }
            }
         }
      }
      emit selectionChanged();
      emit dataChanged(createIndex(0, 0), createIndex(rowCount() - 1, 0), {SelectedRole});
      return;
   }

   --row;
   if (row < fixedEntries_.size()) {
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
   int selStart = row + 1;
   int selEnd = row + 1;

   const bool wasSelectedAddr = (selectionAddresses_.find(data_[row].address))
           != selectionAddresses_.end();
   const bool wasSelectedUtxo = (selectionUtxos_.find({data_[row].txId, data_[row].txOutIndex}))
           != selectionUtxos_.end();

   if (!wasSelectedAddr && !wasSelectedUtxo)  {
      if (!entry.txId.empty()) {
         nbTx_++;
         selectedBalance_ += utxo.getValue();
         selectionUtxos_.insert({utxo.getTxHash(), utxo.getTxOutIndex()});
      }
      else {
         selectionAddresses_.insert(entry.address);
         if (itAddr != utxos_.end()) {
            for (const auto& utxo_addr : itAddr->second) {
               if ((selectionUtxos_.find({utxo_addr.getTxHash(), utxo_addr.getTxOutIndex()}))
                       == selectionUtxos_.end()) {
                  selectedBalance_ += utxo_addr.getValue();
                  selectionUtxos_.insert({utxo_addr.getTxHash(), utxo_addr.getTxOutIndex()});
                  nbTx_++;
               }
            }
            selEnd = row + 1 + itAddr->second.size();
         }
      }
   }
   else if (wasSelectedUtxo) {
      nbTx_--;
      selectedBalance_ -= utxo.getValue();
      int rowParent = row - 1;
      selectionUtxos_.erase({utxo.getTxHash(), utxo.getTxOutIndex()});
      while (rowParent >= 0) {
         if (data_.at(rowParent).expanded) {
            selectionAddresses_.erase(data_.at(rowParent).address);
            break;
         }
         rowParent--;
      }
      if (rowParent >= 0)
      selStart = rowParent + 1;
   }
   else if (wasSelectedAddr){
      selectionAddresses_.erase(entry.address);
      if (itAddr != utxos_.end()) {
         for (const auto& utxo_addr : itAddr->second) {
            if ((selectionUtxos_.find({utxo_addr.getTxHash(), utxo_addr.getTxOutIndex()}))
                    != selectionUtxos_.end()) {
               selectedBalance_ -= utxo_addr.getValue();
               selectionUtxos_.erase({utxo_addr.getTxHash(), utxo_addr.getTxOutIndex()});
               nbTx_--;
            }
         }
         selEnd = row + 1 + itAddr->second.size();
      }
   }
   emit selectionChanged();
   emit dataChanged(createIndex(selStart, 0), createIndex(selEnd, 0), {SelectedRole});
}

QUTXOList* TxInputsModel::getSelection(const QString& address, double amount)
{
   QList<QUTXO*> result;
   if (!address.isEmpty() && (amount > 0) && fixedEntries_.empty() &&  // auto selection
      selectionUtxos_.empty() && selectionAddresses_.empty()) {
      //(static_cast<BTCNumericTypes::satoshi_type>(std::floor(amount * BTCNumericTypes::BalanceDivider)) > selectedBalance_)) {
      const auto& it = preSelected_.find(static_cast<BTCNumericTypes::satoshi_type>(std::floor(amount * BTCNumericTypes::BalanceDivider)));
      if (it != preSelected_.end()) {
         return new QUTXOList(it->second, (QObject*)this);
      }
      if (utxos_.empty()) {
         collectUTXOsForAmount_ = amount;
         return nullptr;
      }
      result = collectUTXOsFor(bs::Address::fromAddressString(address.toStdString()), amount);
      logger_->debug("[{}] auto-selected {} inputs", __func__, result.size());
      preSelected_[static_cast<BTCNumericTypes::satoshi_type>(std::floor(amount * BTCNumericTypes::BalanceDivider))] = result;
   }
   else {
      selectedBalance_ = 0;
      logger_->debug("[{}] {} UTXOs, {} addresses", __func__, selectionUtxos_.size(), selectionAddresses_.size());
      for (const auto& sel : selectionUtxos_) {
         for (const auto& byAddr : utxos_) {
            bool added = false;
            for (const auto& utxo : byAddr.second) {
               if ((sel.first == utxo.getTxHash()) && (sel.second == utxo.getTxOutIndex())) {
                  logger_->debug("[{}] adding {} {}", __func__, sel.first.toHexStr(true), sel.second);
                  selectedBalance_ += utxo.getValue();
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
      for (const auto& sel : selectionAddresses_) {
         const auto& itUTXO = utxos_.find(sel);
         if (itUTXO != utxos_.end()) {
            for (const auto& utxo : itUTXO->second) {
               if (selectionUtxos_.find({utxo.getTxHash(), utxo.getTxOutIndex()})
                  != selectionUtxos_.end()) {
                  continue;
               }
               logger_->debug("[{}] adding {} {}", __func__, utxo.getTxHash().toHexStr(true), utxo.getTxOutIndex());
               selectedBalance_ += utxo.getValue();
               result.push_back(new QUTXO(utxo, (QObject*)this));
            }
         }
      }
      nbTx_ = result.size();
      logger_->debug("[{}] {} UTXOs selected", __func__, nbTx_);
   }
   return new QUTXOList(result, (QObject*)this);
}

void TxInputsModel::updateAutoselection()
{
    //if (static_cast<BTCNumericTypes::satoshi_type>(std::floor(amount * BTCNumericTypes::BalanceDivider)) <= selectedBalance_) {
        //return;
    //}
   if (!selectionUtxos_.empty() || !selectionAddresses_.empty()) {
      return;
   }
   for (int i = data_.size() - 1; i >= 0; --i) {
      const auto& entry = data_[i];
      if (!entry.expanded) {
         toggle(i + 1);
      }
   }

   if (utxos_.empty()) {
      collectUTXOsForAmount_ = true;
      return;
   }
   auto result = collectUTXOsFor();
   selectionUtxos_.clear();
   selectedBalance_ = 0;
   nbTx_ = 0;
   for (const auto utxo : result) {
      selectionUtxos_.insert({ utxo->utxo().getTxHash(), utxo->utxo().getTxOutIndex() });
      logger_->debug("[{}] UTXO selection: {}@{}", __func__, utxo->utxo().getTxHash().toHexStr(true), utxo->utxo().getTxOutIndex());
      selectedBalance_ += utxo->utxo().getValue();
      nbTx_++;
   }

   emit selectionChanged();
   emit dataChanged(createIndex(0, 0), createIndex(rowCount() - 1, 0), { SelectedRole });
}

QUTXOList* TxInputsModel::zcInputs() const
{
   QList<QUTXO*> result;
   for (const auto& entry : data_) {
      logger_->debug("[{}] {}:{} {}", __func__, entry.txId.toHexStr(true), entry.txOutIndex, entry.amount);
      result.push_back(new QUTXO(QUTXO::Input{ entry.txId, entry.amount, entry.txOutIndex }
         , (QObject*)this));
   }
   return new QUTXOList(result, (QObject*)this);
}

QList<QUTXO*> TxInputsModel::collectUTXOsFor(const bs::Address& addr, double amount)
{
   QList<QUTXO*> result;
   std::vector<UTXO> allUTXOs;
   for (const auto& byAddr : utxos_) {
      allUTXOs.insert(allUTXOs.cend(), byAddr.second.cbegin(), byAddr.second.cend());
   }
   bs::Address::decorateUTXOs(allUTXOs);
   const auto& recipients = outsModel_ ? outsModel_->recipients() : decltype(outsModel_->recipients()){};
   std::map<unsigned, std::vector<std::shared_ptr<Armory::Signer::ScriptRecipient>>> recipientsMap;
   for (unsigned i = 0; i < recipients.size(); ++i) {
      recipientsMap[i] = { recipients.at(i) };
   }
   if (!addr.empty() && (amount > 0)) {
      recipientsMap[recipients.size()] = { addr.getRecipient(bs::XBTAmount{amount}) };
   }
   if (recipientsMap.empty()) {
      logger_->error("[TxInputsModel::collectUTXOsFor] no recipients found");
      return result;
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
      if (itUTXOs == utxos_.end()) {
         return QString::number(entry.txOutIndex);
      }
      else {
         return QString::number(itUTXOs->second.size());
      }
      break;
   case ColumnBalance:
      if (itUTXOs != utxos_.end()) {
         uint64_t balance = 0;
         for (const auto& utxo : itUTXOs->second) {
            balance += utxo.getValue();
         }
         return gui_utils::satoshiToQString(balance);
      }
      else {
         if (utxos_.empty()) {
            return gui_utils::satoshiToQString(entry.amount);
         }
         for (const auto& byAddr : utxos_) {
            for (const auto& utxo : byAddr.second) {
               if ((entry.txId == utxo.getTxHash()) && (entry.txOutIndex == utxo.getTxOutIndex())) {
                  return gui_utils::satoshiToQString(utxo.getValue());
               }
            }
         }
      }
      break;
   default: break;
   }
   return {};
}

void TxInputsModel::clearSelection()
{
   selectionUtxos_.clear();
   selectionAddresses_.clear();
   selectedBalance_ = 0.0f;
   nbTx_ = 0;
   setFixedUTXOs(fixedUTXOs_);
   emit dataChanged(createIndex(0, 0), createIndex(rowCount() - 1, 0), { SelectedRole });
}

bool TxInputsModel::isInputSelectable(int row) const
{
   if (row == 0) {
      return true;
   }
   if (row > 0 && row <= fixedEntries_.size()) {
      return false;
   }
   return true;
}
