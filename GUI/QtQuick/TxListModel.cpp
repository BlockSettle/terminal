/*

***********************************************************************************
* Copyright (C) 2020 - 2022, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "TxListModel.h"
#include <fstream>
#include <QDateTime>
#include <spdlog/spdlog.h>
#include "StringUtils.h"
#include "ColorScheme.h"
#include "Utils.h"

namespace {
   static const QHash<int, QByteArray> kTxListRoles{
      {TxListModel::TableDataRole, "tableData"},
      {TxListModel::ColorRole, "dataColor"},
      {TxListModel::TxIdRole, "txId"},
   };
   static const QHash<int, QByteArray> kTxListForAddrRoles{
      {TxListForAddr::TableDataRole, "tableData"},
      {TxListForAddr::ColorRole, "dataColor"},
   };
   static const QHash<int, QByteArray> kTxInOutRoles{
      {TxInOutModel::TableDataRole, "tableData"},
      {TxInOutModel::ColorRole, "dataColor"},
      {TxInOutModel::TxHashRole, "txHash"},
   };

   static const QString dateTimeFormat = QString::fromStdString("yyyy-MM-dd hh:mm:ss");
}

TxListModel::TxListModel(const std::shared_ptr<spdlog::logger>& logger, QObject* parent)
   : QAbstractTableModel(parent), logger_(logger)
   , header_{ tr("Date"), tr("Wallet"), tr("Type"), tr("Address"), tr("Amount")
      , tr("#Conf"), tr("Flag"), tr("Comment") }
{}

int TxListModel::rowCount(const QModelIndex &) const
{
   return data_.size();
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
   const auto& entry = data_.at(row);
   switch (col) {
   case 0:  return QDateTime::fromSecsSinceEpoch(entry.txTime).toString(dateTimeFormat);
   case 1:  return walletName(row);
   case 2:  return txType(row);
   case 3: {
      bs::Address address;
      if (!entry.addresses.empty()) {
         address = *entry.addresses.cbegin();
      }
      return QString::fromStdString(address.display());
   }
   case 4: return gui_utils::balance_to_qstring(std::abs(entry.value));
   case 5: return QString::number(entry.nbConf);
   case 6: return txFlag(row);
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
   const auto& entry = data_.at(row);
   if (col == 5) {
      switch (entry.nbConf) {
      case 0:  return ColorScheme::transactionConfirmationZero;
      case 1:
      case 2:
      case 3:
      case 4:
      case 5:  return QColorConstants::Yellow;
      default: return ColorScheme::transactionConfirmationHigh;
      }
   }
   else if (col == 2) {
      const auto& itTxDet = txDetails_.find(row);
      if (itTxDet != txDetails_.end()) {
         switch (itTxDet->second.direction) {
         case bs::sync::Transaction::Direction::Received:   return ColorScheme::transactionConfirmationHigh;
         case bs::sync::Transaction::Direction::Sent:       return ColorScheme::transactionConfirmationZero;
         case bs::sync::Transaction::Direction::Internal:   return QColorConstants::Cyan;
         default: break;
         }
      }
      return QColorConstants::White;
   }
   else if (col == 4) {
      if (entry.value < 0) {
         return qRgb(208, 192, 192);
      }
   }
   return QColorConstants::White;
}

QString TxListModel::walletName(int row) const
{
   const auto& itTxDet = txDetails_.find(row);
   if (itTxDet != txDetails_.end()) {
      return QString::fromStdString(itTxDet->second.walletName);
   }
   return {};
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

QString TxListModel::txId(int row) const
{
   const auto& itTxDet = txDetails_.find(row);
   if (itTxDet != txDetails_.end()) {
      return QString::fromStdString(itTxDet->second.txHash.toHexStr(true));
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
   case ColorRole:
      return dataColor(index.row(), index.column());
   case TxIdRole:
      return txId(index.row());
   default: break;
   }
   return QVariant();
}

QVariant TxListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
   if (orientation == Qt::Orientation::Horizontal) {
      return header_.at(section);
   }
   return QVariant();
}

QHash<int, QByteArray> TxListModel::roleNames() const
{
   return kTxListRoles;
}

void TxListModel::addRows(const std::vector<bs::TXEntry>& entries)
{
   std::vector<bs::TXEntry> newEntries;
   for (const auto& entry : entries) {
      int row = -1;
      for (int i = 0; i < data_.size(); ++i) {
         const auto& de = data_.at(i);
         if ((entry.txHash == de.txHash) && (de.walletIds == entry.walletIds)) {
            data_[i].txTime = entry.txTime;
            data_[i].recvTime = entry.recvTime;
            row = i;
            break;
         }
      }
      if (row != -1) {
         //logger_->debug("[{}::{}] updating entry {}", (void*)this, __func__, entry.txHash.toHexStr(true));
         emit dataChanged(createIndex(row, 0), createIndex(row, 0));
      }
      else {
         //logger_->debug("[{}::{}] adding entry {}", (void*)this, __func__, entry.txHash.toHexStr(true));
         newEntries.push_back(entry);
      }
   }
   if (!newEntries.empty()) {
      beginInsertRows(QModelIndex(), rowCount(), rowCount() + newEntries.size() - 1);
      data_.insert(data_.end(), newEntries.cbegin(), newEntries.cend());
      endInsertRows();
      emit nbTxChanged();
   }
}

void TxListModel::prependRow(const bs::TXEntry& entry)
{
   //logger_->debug("[{}::{}] prepending entry {}", (void*)this, __func__, entry.txHash.toHexStr(true));
   decltype(txDetails_) prevDet;
   beginInsertRows(QModelIndex(), 0, 0);
   data_.insert(data_.cbegin(), entry);
   txDetails_.swap(prevDet);
   for (auto txDet : prevDet) {
      txDetails_[txDet.first] = std::move(txDet.second);
   }
   endInsertRows();
   emit nbTxChanged();
}

void TxListModel::addRow(const bs::TXEntry& entry)
{
   int row = -1;
   for (int i = 0; i < data_.size(); ++i) {
      const auto& de = data_.at(i);
      if ((entry.txHash == de.txHash) && (de.walletIds == entry.walletIds)) {
         data_[i].txTime = entry.txTime;
         data_[i].recvTime = entry.recvTime;
         row = i;
         break;
      }
   }

   if (row != -1) {
      //logger_->debug("[{}::{}] updating entry {}", (void*)this, __func__, entry.txHash.toHexStr(true));
      emit dataChanged(createIndex(row, 0), createIndex(row, 0));
   }
   else {
      //logger_->debug("[{}::{}] adding entry {}", (void*)this, __func__, entry.txHash.toHexStr(true));
      beginInsertRows(QModelIndex(), rowCount(), rowCount());
      data_.push_back(entry);
      endInsertRows();
      emit nbTxChanged();
   }
}

void TxListModel::clear()
{
   beginResetModel();
   data_.clear();
   txDetails_.clear();
   endResetModel();
   emit nbTxChanged();
}

void TxListModel::setTxComment(const std::string& txHash, const std::string& comment)
{
   txComments_[txHash] = comment;
   int rowFirst = -1, rowLast = -1;
   for (int i = 0; i < data_.size(); ++i) {
      const auto& txEntryHash = data_.at(i).txHash.toBinStr();
      if (txHash == txEntryHash) {
         if (!rowFirst) {
            rowFirst = i;
         }
         rowLast = i;
      }
   }
   if (rowFirst != -1 && rowLast != -1) {
      emit dataChanged(createIndex(rowFirst, 7), createIndex(rowLast, 7));
   }
}

void TxListModel::setDetails(const bs::sync::TXWalletDetails& txDet)
{
   int row = -1;
   for (int i = 0; i < data_.size(); ++i) {
      const auto& entry = data_.at(i);
      if ((entry.txHash == txDet.txHash) && (txDet.walletIds == entry.walletIds)) {
         txDetails_[i] = txDet;
         data_[i].addresses = txDet.ownAddresses;
         row = i;
         break;
      }
   }
   if (row != -1) {
      emit dataChanged(createIndex(row, 1), createIndex(row, 6));
      //logger_->debug("[{}] {} {} found at row {}", __func__, txDet.txHash.toHexStr(), txDet.hdWalletId, row);
      
   }
   else {
      //logger_->debug("[{}] {} {} not found", __func__, txDet.txHash.toHexStr(), txDet.hdWalletId);
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
   emit dataChanged(createIndex(0, 5), createIndex(data_.size() - 1, 5));
}

bool TxListModel::exportCSVto(const QString& filename)
{
   const std::string& prefix = "file:///";
   std::string fileName = filename.toStdString();
   const auto itPrefix = fileName.find(prefix);
   if (itPrefix != std::string::npos) {
      fileName.replace(itPrefix, prefix.size(), "");
   }
   logger_->debug("[{}] {}", __func__, fileName);
   std::ofstream fstrm(fileName);
   if (!fstrm.is_open()) {
      return false;
   }
   fstrm << "sep=;\nTimestamp;Wallet;Type;Address;TxId;Amount;Comment\n";
   for (int i = 0; i < rowCount(); ++i) {
      const auto& entry = data_.at(i);
      std::time_t txTime = entry.txTime;
      fstrm << "\"" << std::put_time(std::localtime(&txTime), "%Y-%m-%d %X") << "\";"
         << "\"" << getData(i, 1).toUtf8().toStdString() << "\";"
         << getData(i, 2).toStdString() << ";"
         << getData(i, 3).toStdString() << ";"
         << txId(i).toStdString() << ";"
         << fmt::format("{:.8f}", entry.value / BTCNumericTypes::BalanceDivider) << ";"
         << "\"" << getData(i, 7).toStdString() << "\"\n";
   }
   return true;
}


TxListForAddr::TxListForAddr(const std::shared_ptr<spdlog::logger>& logger, QObject* parent)
   : QAbstractTableModel(parent), logger_(logger)
   , header_{ tr("Date"), tr("Transaction ID"), tr("#Conf"), tr("#Ins"), tr("#Outs"), tr("Amount (BTC)")
      , tr("Fees (BTC)"), tr("fpb"), tr("VSize (B)") }
{}

int TxListForAddr::rowCount(const QModelIndex&) const
{
   return data_.size();
}

int TxListForAddr::columnCount(const QModelIndex&) const
{
   return header_.size();
}

static QString displayNb(int nb)
{
   if (nb < 0) {
      return QObject::tr("...");
   }
   return QString::number(nb);
}

static QString displayBTC(double btc, int precision = 8)
{
   if (btc < 0) {
      return QObject::tr("...");
   }
   return QString::number(btc, 'f', precision);
}

QString TxListForAddr::getData(int row, int col) const
{
   if (row > data_.size()) {
      return {};
   }
   const auto& entry = data_.at(row);
   const auto totFees = totalFees(row);
   switch (col) {
   case 0:  return QDateTime::fromSecsSinceEpoch(entry.txTime).toString(dateTimeFormat);
   case 1:  return txId(row);
   case 2:  return QString::number(entry.nbConf);
   case 3:  return displayNb(nbInputs(row));
   case 4:  return displayNb(nbOutputs(row));
   case 5:  return gui_utils::balance_to_qstring(entry.value);
   case 6:  return displayBTC(totFees / BTCNumericTypes::BalanceDivider);
   case 7:  return (totFees < 0) ? tr("...") : displayBTC(totFees / (double)txSize(row), 1);
   case 8:  return displayNb(txSize(row));
   default: break;
   }
   return {};
}

int TxListForAddr::nbInputs(int row) const
{
   const auto& itTxDet = txs_.find(row);
   if (itTxDet != txs_.end()) {
      return itTxDet->second.getNumTxIn();
   }
   return -1;
}

int TxListForAddr::nbOutputs(int row) const
{
   const auto& itTxDet = txs_.find(row);
   if (itTxDet != txs_.end()) {
      return itTxDet->second.getNumTxOut();
   }
   return -1;
}

int TxListForAddr::txSize(int row) const
{
   const auto& itTxDet = txs_.find(row);
   if (itTxDet != txs_.end()) {
      return itTxDet->second.getTxWeight();
   }
   return -1;
}

int64_t TxListForAddr::totalFees(int row) const
{
   const auto& itTxDet = txs_.find(row);
   if (itTxDet != txs_.end()) {
      int64_t txValue = 0;
      for (int i = 0; i < itTxDet->second.getNumTxIn(); ++i) {
         const auto& in = itTxDet->second.getTxInCopy(i);
         const OutPoint op = in.getOutPoint();
         const auto& itInput = inputs_.find(op.getTxHash());
         if (itInput == inputs_.end()) {
            return -1;
         }
         const auto& prevOut = itInput->second.getTxOutCopy(op.getTxOutIndex());
         txValue += prevOut.getValue();
      }
      for (int i = 0; i < itTxDet->second.getNumTxOut(); ++i) {
         const auto& out = itTxDet->second.getTxOutCopy(i);
         txValue -= out.getValue();
      }
      return txValue;
   }
   return -1;
}

QColor TxListForAddr::dataColor(int row, int col) const
{
   const auto& entry = data_.at(row);
   if (col == 2) {
      switch (entry.nbConf) {
      case 0:  return ColorScheme::transactionConfirmationZero;
      case 1:
      case 2:
      case 3:
      case 4:
      case 5:  return QColorConstants::Yellow;
      default: return ColorScheme::transactionConfirmationHigh;
      }
   }
   return QColorConstants::White;
}

QString TxListForAddr::txId(int row) const
{
   try {
      return QString::fromStdString(data_.at(row).txHash.toHexStr(true));
   }
   catch (const std::exception&) {}
   return {};
}

QVariant TxListForAddr::data(const QModelIndex& index, int role) const
{
   if (index.column() >= header_.size()) {
      return {};
   }
   switch (role) {
   case TableDataRole:
      return getData(index.row(), index.column());
   case ColorRole:
      return dataColor(index.row(), index.column());
   default: break;
   }
   return QVariant();
}

QVariant TxListForAddr::headerData(int section, Qt::Orientation orientation, int role) const
{
   if (orientation == Qt::Orientation::Horizontal) {
      return header_.at(section);
   }
   return QVariant();
}

QHash<int, QByteArray> TxListForAddr::roleNames() const
{
   return kTxListForAddrRoles;
}

void TxListForAddr::addRows(const std::vector<bs::TXEntry>& entries)
{
   if (!entries.empty()) {
      beginInsertRows(QModelIndex(), rowCount(), rowCount() + entries.size() - 1);
      data_.insert(data_.end(), entries.cbegin(), entries.cend());
      endInsertRows();
   }
   emit changed();
}

void TxListForAddr::clear()
{
   beginResetModel();
   data_.clear();
   txs_.clear();
   endResetModel();
   emit changed();
}

void TxListForAddr::setDetails(const std::vector<Tx>& txs)
{
   int rowStart = -1, rowEnd = -1;
   for (const auto& tx : txs) {
      for (int i = 0; i < data_.size(); ++i) {
         const auto& entry = data_.at(i);
         if (entry.txHash == tx.getThisHash()) {
            txs_[i] = tx;
            if (!rowStart) {
               rowStart = i;
            }
            if (rowEnd <= i) {
               rowEnd = i;
            }
         }
      }
   }
   if (rowStart != -1 && rowEnd != -1) {
      emit dataChanged(createIndex(rowStart, 3), createIndex(rowEnd, 8));
   }
}

void TxListForAddr::setInputs(const std::vector<Tx>& txs)
{
   for (const auto& tx : txs) {
      inputs_[tx.getThisHash()] = tx;
   }
   emit dataChanged(createIndex(0, 6), createIndex(data_.size() - 1, 7));
}

void TxListForAddr::setCurrentBlock(uint32_t nbBlock)
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
   emit dataChanged(createIndex(0, 2), createIndex(data_.size() - 1, 2));
}

QString TxListForAddr::totalReceived() const
{
   int64_t result = 0;
   for (const auto& entry : data_) {
      if (entry.value > 0) {
         result += entry.value;
      }
   }
   return displayBTC(result / BTCNumericTypes::BalanceDivider);
}

QString TxListForAddr::totalSent() const
{
   int64_t result = 0;
   for (const auto& entry : data_) {
      if (entry.value < 0) {
         result += entry.value;
      }
   }
   return displayBTC(std::abs(result) / BTCNumericTypes::BalanceDivider);
}

QString TxListForAddr::balance() const
{
   int64_t result = 0;
   for (const auto& entry : data_) {
      result += entry.value;
   }
   return displayBTC(result / BTCNumericTypes::BalanceDivider);
}


void QTxDetails::setDetails(const bs::sync::TXWalletDetails& details)
{
   details_ = details;
   if (details.changeAddress.address.isValid()) {
      details_.outputAddresses.push_back(details.changeAddress);
   }
   QMetaObject::invokeMethod(this, [this] {
      inputsModel_ = new TxInOutModel(details_.inputAddresses, tr("Input"), this);
      outputsModel_ = new TxInOutModel(details_.outputAddresses, tr("Output"), this);
      emit updated();
   });
}

void QTxDetails::setCurBlock(uint32_t curBlock)
{
   if (curBlock_ != curBlock) {
      curBlock_ = curBlock;
      QMetaObject::invokeMethod(this, [this] {
         emit newBlock();
      });
   }
}

QString QTxDetails::virtSize() const
{
   return displayNb(details_.tx.getTxWeight());
}

QString QTxDetails::nbConf() const
{
   const int txHeight = details_.tx.getTxHeight();
   return displayNb((txHeight > 0) ? curBlock_ - txHeight + 1 : txHeight);
}

QString QTxDetails::nbInputs() const
{
   return displayNb(details_.tx.getNumTxIn());
}

QString QTxDetails::nbOutputs() const
{
   return displayNb(details_.tx.getNumTxOut());
}

QString QTxDetails::inputAmount() const
{
   uint64_t amount = 0;
   for (const auto& in : details_.inputAddresses) {
      amount += in.value;
   }
   return displayBTC(amount / BTCNumericTypes::BalanceDivider);
}

QString QTxDetails::outputAmount() const
{
   uint64_t amount = 0;
   for (const auto& out : details_.outputAddresses) {
      amount += out.value;
   }
   return displayBTC(amount / BTCNumericTypes::BalanceDivider);
}

QString QTxDetails::fee() const
{
   int64_t amount = 0;
   for (const auto& in : details_.inputAddresses) {
      amount += in.value;
   }
   for (const auto& out : details_.outputAddresses) {
      amount -= out.value;
   }
   return displayBTC(amount / BTCNumericTypes::BalanceDivider);
}

QString QTxDetails::feePerByte() const
{
   int64_t amount = 0;
   for (const auto& in : details_.inputAddresses) {
      amount += in.value;
   }
   for (const auto& out : details_.outputAddresses) {
      amount -= out.value;
   }
   int txWeight = details_.tx.getTxWeight();
   if (!txWeight) {
      txWeight = -1;
   }
   return displayBTC(amount / txWeight, 1);
}

quint32 QTxDetails::height() const
{
   return details_.tx.getTxHeight();
}

TxInOutModel::TxInOutModel(const std::vector<bs::sync::AddressDetails>& data, const QString& type, QObject* parent)
   : QAbstractTableModel(parent)
   , data_(data)
   , type_(type)
   , header_{ tr("Type"), tr("Address"), tr("Amount"), tr("Wallet") }
{}

int TxInOutModel::rowCount(const QModelIndex&) const
{
   return data_.size();
}

int TxInOutModel::columnCount(const QModelIndex&) const
{
   return header_.size();
}

QVariant TxInOutModel::data(const QModelIndex& index, int role) const
{
   if (index.column() >= header_.size()) {
      return {};
   }
   switch (role) {
   case TableDataRole:
      return getData(index.row(), index.column());
   case ColorRole:
      return dataColor(index.row(), index.column());
   case TxHashRole:
      try {
         return QString::fromStdString(data_.at(index.row()).outHash.toHexStr(true));
      }
      catch (const std::exception&) { return {}; }
   default: break;
   }
   return {};
}

QVariant TxInOutModel::headerData(int section, Qt::Orientation orientation, int role) const
{
   if (orientation == Qt::Orientation::Horizontal) {
      return header_.at(section);
   }
   return QVariant();
}

QHash<int, QByteArray> TxInOutModel::roleNames() const
{
   return kTxInOutRoles;
}

QString TxInOutModel::getData(int row, int col) const
{
   if (row > data_.size()) {
      return {};
   }
   try {
      switch (col) {
      case 0: return type_;
      case 1: return QString::fromStdString(data_.at(row).address.display());
      case 2: return QString::fromStdString(data_.at(row).valueStr);
      case 3: return QString::fromStdString(data_.at(row).walletName);
      default: break;
      }
   }
   catch (const std::exception&) {
      return {};
   }
   return {};
}

QColor TxInOutModel::dataColor(int row, int col) const
{
   return QColorConstants::White;
}
