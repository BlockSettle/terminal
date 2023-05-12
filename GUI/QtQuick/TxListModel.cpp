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
#include "ColorScheme.h"
#include "StringUtils.h"
#include "TxInputsSelectedModel.h"
#include "TxOutputsModel.h"
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

   QString getTime2String(std::time_t& t)
   {
      std::tm* tm = std::localtime(&t);
      char buffer[50];

      std::strftime(buffer, sizeof(buffer), "%y%m%d", tm);
      return QString::fromStdString(std::string(buffer));   
   }
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

QVariant TxListModel::getData(int row, int col) const
{
   if (row > data_.size()) {
      return {};
   }
   const auto& entry = data_.at(row);
   switch (col) {
   case 0:  return QDateTime::fromSecsSinceEpoch(entry.txTime).toString(gui_utils::dateTimeFormat);
   case 1:  return walletName(row);
   case 2:  return txType(row);
   case 3: {
      bs::Address address;
      if (!entry.addresses.empty()) {
         address = *entry.addresses.cbegin();
      }
      return QString::fromStdString(address.display());
   }
   case 4: return gui_utils::satoshiToQString(std::abs(entry.value));
   case 5: return entry.nbConf;
   case 6: return txFlag(row);
   case 7: return txComment(row);
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

bs::sync::Transaction::Direction TxListModel::txDirection(int row) const
{
   const auto& itTxDet = txDetails_.find(row);
   if (itTxDet != txDetails_.end()) {
      return itTxDet->second.direction;
   }
   return bs::sync::Transaction::Unknown;
}

QString TxListModel::txType(int row) const
{
   return gui_utils::directionToQString(txDirection(row));
}

QString TxListModel::txComment(int row) const
{
   const auto& itTxDet = txDetails_.find(row);
   if (itTxDet != txDetails_.end()) {
      return QString::fromStdString(itTxDet->second.comment);
   }
   return {};
}

QString TxListModel::txFlag(int row) const
{
   if (isRBF(row)) {
      return tr("RBF");
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

bool TxListModel::isRBF(int row) const
{
   const auto& itTxDet = txDetails_.find(row);
   if (itTxDet != txDetails_.end()) {
      return itTxDet->second.tx.isRBF();
   }
   return false;
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
   case RBFRole:
      return isRBF(index.row());
   case NbConfRole:
      return  data_.at(index.row()).nbConf;
   case DirectionRole:
       return txDirection(index.row());
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
   logger_->debug("[TxListModel::addRows] {} entries", entries.size());
   std::vector<bs::TXEntry> newEntries;
   for (const auto& entry : entries) {
      int row = -1;
      for (int i = 0; i < data_.size(); ++i) {
         auto& de = data_.at(i);
         if ((entry.txHash == de.txHash) && (de.walletIds == entry.walletIds)) {
            de.txTime = entry.txTime;
            de.recvTime = entry.recvTime;
            row = i;
            break;
         }
      }
      if (row != -1) {
         //logger_->debug("[{}] updating entry #{} {}", __func__, row, entry.txHash.toHexStr(true));
         emit dataChanged(createIndex(row, 0), createIndex(row, 0));
      }
      else {
         //logger_->debug("[{}::{}] adding entry {}", (void*)this, __func__, entry.txHash.toHexStr(true));
         newEntries.push_back(entry);
      }
   }
   if (!newEntries.empty()) {
      QMetaObject::invokeMethod(this, [this, newEntries] {
         logger_->debug("[TxListModel::addRows] {} new entries", newEntries.size());
         {
            beginInsertRows(QModelIndex(), rowCount(), rowCount() + newEntries.size() - 1);
            std::unique_lock lock{ dataMtx_ };
            data_.insert(data_.end(), newEntries.cbegin(), newEntries.cend());
            endInsertRows();
         }
         emit nbTxChanged();

         if (!pendingDetails_.empty()) {
            logger_->debug("[TxListModel::addRows] {} pending details", pendingDetails_.size());
            while (!pendingDetails_.empty()) {
               std::unique_lock lock{ dataMtx_ };
               auto txDet = *pendingDetails_.rbegin();
               pendingDetails_.pop_back();
               lock.unlock();
               setDetails(txDet, false);
            }
         }
         });
   }
}

void TxListModel::clear()
{
   beginResetModel();
   std::unique_lock lock{ dataMtx_ };
   data_.clear();
   txDetails_.clear();
   endResetModel();
   emit nbTxChanged();
}

void TxListModel::setDetails(const bs::sync::TXWalletDetails& txDet, bool usePending)
{
   int row = -1;
   std::unique_lock lock{ dataMtx_ };
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
      emit dataChanged(createIndex(row, 1), createIndex(row, 7));
      //logger_->debug("[TxListModel::setDetails] {} {} found at row {}", txDet.txHash.toHexStr(), txDet.hdWalletId, row);
      
   }
   else {
      //logger_->warn("[TxListModel::setDetails] {} {} not found", txDet.txHash.toHexStr(), txDet.hdWalletId);
      if (usePending) {
         pendingDetails_.push_back(txDet);
      }
   }
}

void TxListModel::removeTX(const BinaryData& txHash)
{
   int rowStart = -1, rowEnd = -1;
   for (int i = 0; i < data_.size(); ++i) {
      const auto& entry = data_.at(i);
      if (entry.txHash == txHash) {
         if (rowStart < 0) {
            rowStart = i;
         }
         rowEnd = i;
      }
   }
   if ((rowEnd < 0) || (rowStart < 0)) {
      logger_->warn("[{}] TX {} not found", __func__, txHash.toHexStr(true));
      return;
   }
   logger_->debug("[{}] {}: start: {}, end: {}", __func__, txHash.toHexStr(true), rowStart, rowEnd);
   QMetaObject::invokeMethod(this, [this, rowStart, rowEnd] {
      if (rowStart >= rowCount()) {
         return;
      }
      beginRemoveRows(QModelIndex(), rowStart, rowEnd);
      std::unique_lock lock{ dataMtx_ };
      data_.erase(data_.cbegin() + rowStart, data_.cbegin() + rowEnd + 1);
      for (int i = rowEnd + 1; i < txDetails_.size(); i++) {
         txDetails_[i - (rowEnd - rowStart) - 1] = std::move(txDetails_[i]);
      }
      endRemoveRows();
   });
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
   std::unique_lock writeLock{ dataMtx_ };
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
         << "\"" << getData(i, 1).toString().toUtf8().toStdString() << "\";"
         << getData(i, 2).toString().toStdString() << ";"
         << getData(i, 3).toString().toStdString() << ";"
         << txId(i).toStdString() << ";"
         << fmt::format("{:.8f}", entry.value / BTCNumericTypes::BalanceDivider) << ";"
         << "\"" << getData(i, 7).toString().toStdString() << "\"\n";
   }
   return true;
}

QString TxListModel::getBegDate() const
{
   std::time_t minTime = std::time(nullptr);

   for (int i = 0; i < rowCount(); ++i) {
      const auto& entry = data_.at(i);

      if (minTime > entry.txTime)
         minTime = entry.txTime;
   }

   return getTime2String(minTime);
}

QString TxListModel::getEndDate() const
{
   std::time_t maxTime = std::time(nullptr);
   
   return getTime2String(maxTime);
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
   case 0:  return QDateTime::fromSecsSinceEpoch(entry.txTime).toString(gui_utils::dateTimeFormat);
   case 1:  return txId(row);
   case 2:  return QString::number(entry.nbConf);
   case 3:  return displayNb(nbInputs(row));
   case 4:  return displayNb(nbOutputs(row));
   case 5:  return gui_utils::satoshiToQString(entry.value);
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
      QMetaObject::invokeMethod(this, [this, entries] {
         beginInsertRows(QModelIndex(), rowCount(), rowCount() + entries.size() - 1);
         data_.insert(data_.end(), entries.crbegin(), entries.crend());
         endInsertRows();
         emit changed();
         });
   }
}

void TxListForAddr::clear()
{
   QMetaObject::invokeMethod(this, [this] {
      beginResetModel();
      data_.clear();
      txs_.clear();
      endResetModel();
      emit changed();
      });
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


QTxDetails::QTxDetails(const std::shared_ptr<spdlog::logger>& logger, const BinaryData& txHash, QObject* parent)
   : QObject(parent), logger_(logger), txHash_(txHash)
{
   outputsModel_ = new TxOutputsModel(logger_, this);
   inputsModel_ = new TxInputsModel(logger_, outputsModel_, this);
   selInputsModel_ = new TxInputsSelectedModel(this);
   selInputsModel_->setSourceModel(inputsModel_);
   inputs_ = new TxInOutModel(tr("Input"), this);
   outputs_ = new TxInOutModel(tr("Output"), this);
}

void QTxDetails::setDetails(const bs::sync::TXWalletDetails& details)
{
   details_ = details;
   if (details.changeAddress.address.isValid()) {
      details_.outputAddresses.push_back(details.changeAddress);
   }
   inputs_->setData(details_.inputAddresses);
   outputs_->setData(details_.outputAddresses);
   outputsModel_->clearOutputs();
   inputsModel_->clear();
   if (!needInputsFromOutputs_) {
      for (const auto& out : details.outputAddresses) {
         outputsModel_->addOutput(QString::fromStdString(out.address.display())
            , out.value / BTCNumericTypes::BalanceDivider);
      }
   }
   ins_.clear();
   ins_.reserve(details.inputAddresses.size());
   for (const auto& in : details.inputAddresses) {
      logger_->debug("[{}] in: {} {}@{} = {}", __func__, in.address.display()
         , in.outHash.toHexStr(true), in.outIndex, in.value);
      ins_.push_back({ in.address, in.outHash, in.outIndex, in.value });
   }
   if (!fixedInputs_.empty()) {
      setImmutableUTXOs(fixedInputs_);
      fixedInputs_.clear();
   }

   outs_.clear();
   outs_.reserve(details.outputAddresses.size());
   const auto& txId = details_.tx.isInitialized() ? details_.tx.getThisHash() : BinaryData{};
   for (const auto& out : details.outputAddresses) {
      outs_.push_back({ out.address, txId, out.outIndex, out.value });
   }
   if (needInputsFromOutputs_) {
      setInputsFromOutputs();
   }
   else {
      inputsModel_->addEntries(ins_);
   }
   emit updated();
}

void QTxDetails::setImmutableUTXOs(const std::vector<UTXO>& utxos)
{
   if (ins_.empty()) {
      fixedInputs_ = utxos;
      return;
   }
   logger_->debug("[{}] {}", __func__, utxos.size());
   inputsModel_->setFixedInputs(ins_);
   if (!inputsModel_->setFixedUTXOs(utxos)) {
      fixedInputs_ = utxos;
   }
}

void QTxDetails::setInputsFromOutputs()
{
   if (outs_.empty()) {
      logger_->info("[{}] no outputs received [yet]");
      needInputsFromOutputs_ = true;
      return;
   }
   decltype(outs_) outs;
   outs.reserve(details_.outputAddresses.size());
   const auto& txId = details_.tx.isInitialized() ? details_.tx.getThisHash() : BinaryData{};
   for (const auto& out : details_.outputAddresses) {
      outs.push_back({ out.address, txId, out.outIndex, out.value });
   }

   logger_->debug("[{}] {} outs", __func__, outs.size());
   for (const auto& out : outs) {
      logger_->debug("[{}] {} {}@{} = {}", __func__, out.address.display()
         , out.txId.toHexStr(true), out.txOutIndex, out.amount);
   }
   inputsModel_->setFixedInputs(outs);
   ins_ = std::move(outs);
   needInputsFromOutputs_ = false;

   if (!fixedInputs_.empty()) {
      setImmutableUTXOs(fixedInputs_);
      fixedInputs_.clear();
   }
}

std::vector<std::pair<bs::Address, double>> QTxDetails::outputData() const
{
   std::vector<std::pair<bs::Address, double>> result;
   for (const auto& out : outs_) {
      result.push_back({out.address, out.amount / BTCNumericTypes::BalanceDivider});
   }
   return result;
}

bool QTxDetails::amountsMatch(float fpb) const
{
   if (outputsModel_->rowCount() <= 1) {
      return false;
   }
   const auto inBalance = inputsModel_->balanceValue();
   if (inBalance < 0.0000001) {
      return true;
   }
   const float fpbCur = feePerByteValue();
   logger_->debug("[{}] fpb={}/{}, feeVal={}", __func__, fpb, fpbCur, feeValue());
   const auto outBalance = outputsModel_->totalAmount();
   const float fee = fpb ? (feeValue() * fpb / (fpbCur ? fpbCur : 1)) : feeValue();
   double diff = inBalance - outBalance - fee;
   logger_->debug("[{}] in:{:.8f}, out:{:.8f}, fee:{:.8f}, diff={} fpb={}", __func__, inBalance
      , outBalance, fee, diff, fpb);
   return ((diff > 0) || (std::abs(diff) < 0.000003));
}

void QTxDetails::onTopBlock(quint32 curBlock)
{
   if (curBlock_ != curBlock) {
      curBlock_ = curBlock;
      if (inputsModel_) {
         inputsModel_->setTopBlock(curBlock);
      }
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
   const auto txHeight = details_.tx.getTxHeight();
   return (txHeight != UINT32_MAX) ? displayNb(curBlock_ - txHeight + 1) : displayNb(0);
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

float QTxDetails::feeValue() const
{
   int64_t amount = 0;
   for (const auto& in : details_.inputAddresses) {
      amount += in.value;
   }
   for (const auto& out : details_.outputAddresses) {
      amount -= out.value;
   }
   return amount / BTCNumericTypes::BalanceDivider;
}

QString QTxDetails::fee() const
{
   return displayBTC(feeValue());
}

float QTxDetails::feePerByteValue() const
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
   return amount / txWeight;
}
QString QTxDetails::feePerByte() const
{
   return displayBTC(feePerByteValue(), 1);
}

QString QTxDetails::timestamp() const
{
   return QDateTime::fromSecsSinceEpoch(details_.tx.getTxTime())
      .toString(gui_utils::dateTimeFormat);
}

QString QTxDetails::height() const
{
   return (details_.tx.getTxHeight() != UINT32_MAX) ? displayNb(details_.tx.getTxHeight())
      : tr("-");
}

bs::sync::Transaction::Direction QTxDetails::direction() const
{
   return details_.direction;
}

TxInOutModel::TxInOutModel(const QString& type, QObject* parent)
   : QAbstractTableModel(parent)
   , type_(type)
   , header_{ tr("Type"), tr("Address"), tr("Amount"), tr("Wallet") }
{}

void TxInOutModel::setData(const std::vector<bs::sync::AddressDetails>& data)
{
   beginResetModel();
   data_ = data;
   endResetModel();
}

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
