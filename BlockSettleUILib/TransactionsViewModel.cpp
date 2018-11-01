#include "TransactionsViewModel.h"

#include "ArmoryConnection.h"
#include "CheckRecipSigner.h"
#include "UiUtils.h"
#include "WalletsManager.h"

#include <QDateTime>
#include <QMutexLocker>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>


TransactionsViewModel::TransactionsViewModel(const std::shared_ptr<ArmoryConnection> &armory, const std::shared_ptr<WalletsManager> &walletsManager
   , const AsyncClient::LedgerDelegate &ledgerDelegate, QObject* parent, const std::shared_ptr<bs::Wallet> &defWlt)
   : QAbstractTableModel(parent)
   , armory_(armory)
   , walletsManager_(walletsManager)
   , ledgerDelegate_(ledgerDelegate)
   , defaultWallet_(defWlt)
   , allWallets_(false)
{
   init();
   QtConcurrent::run(this, &TransactionsViewModel::loadLedgerEntries);
}

TransactionsViewModel::TransactionsViewModel(const std::shared_ptr<ArmoryConnection> &armory, const std::shared_ptr<WalletsManager> &walletsManager
   , QObject* parent)
   : QAbstractTableModel(parent)
   , armory_(armory)
   , walletsManager_(walletsManager)
   , allWallets_(true)
{
   init();
}

void TransactionsViewModel::init()
{
   stopped_ = false;
   initialLoadCompleted_ = true;
   colorGray_ = Qt::darkGray, colorRed_ = Qt::red, colorYellow_ = Qt::darkYellow;
   colorGreen_ = Qt::darkGreen, colorInvalid_ = Qt::red;
   fontBold_.setBold(true);
   qRegisterMetaType<TransactionsViewItem>();
   qRegisterMetaType<TransactionItems>();

   if (armory_) {
      connect(armory_.get(), SIGNAL(stateChanged(ArmoryConnection::State)), this, SLOT(onArmoryStateChanged(ArmoryConnection::State)), Qt::QueuedConnection);
      connect(armory_.get(), &ArmoryConnection::newBlock, this, &TransactionsViewModel::updatePage, Qt::QueuedConnection);
   }
   connect(walletsManager_.get(), &WalletsManager::walletChanged, this, &TransactionsViewModel::refresh, Qt::QueuedConnection);
   connect(walletsManager_.get(), &WalletsManager::walletsReady, this, &TransactionsViewModel::updatePage, Qt::QueuedConnection);
   connect(walletsManager_.get(), &WalletsManager::newTransactions, this, &TransactionsViewModel::onNewTransactions, Qt::QueuedConnection);

   cmdTimer_ = new QTimer(this);
   cmdTimer_->setSingleShot(false);
   cmdTimer_->setInterval(100);
   connect(cmdTimer_, &QTimer::timeout, this, &TransactionsViewModel::timerCmd);
   cmdTimer_->start();
}

TransactionsViewModel::~TransactionsViewModel()
{
   stopped_ = true;
}

void TransactionsViewModel::loadAllWallets()
{
   const auto &cbWalletsLD = [this](AsyncClient::LedgerDelegate delegate) {
      ledgerDelegate_ = delegate;
      QMetaObject::invokeMethod(this, [this] {
         QtConcurrent::run(this, &TransactionsViewModel::loadLedgerEntries);
      });
   };
   armory_->getWalletsLedgerDelegate(cbWalletsLD);
}

int TransactionsViewModel::columnCount(const QModelIndex &) const
{
   return static_cast<int>(Columns::last);
}

int TransactionsViewModel::rowCount(const QModelIndex &parent) const
{
   if (parent.isValid()) {
      return 0;
   }
   return static_cast<int>(currentPage_.size());
}

QVariant TransactionsViewModel::data(const QModelIndex &index, int role) const
{
   const auto col = static_cast<Columns>(index.column());

   if (role == Qt::TextAlignmentRole) {
      switch (col) {
      case Columns::Amount:   return Qt::AlignRight;
      case Columns::RbfFlag:  return Qt::AlignCenter;
      default:  break;
      }
      return {};
   }

   const unsigned int row = index.row();
   if (row >= currentPage_.size()) {
      return {};
   }
   const auto &item = currentPage_[row];

   if (role == Qt::DisplayRole) {
      switch(col) {
      case Columns::Date:
         return item.displayDateTime;
      case Columns::Status:
         return tr("   %1").arg(item.confirmations);
      case Columns::Wallet:
         return item.walletName;
      case Columns::SendReceive:
         return item.dirStr;
      case Columns::Comment:
         return item.comment;
      case Columns::Amount:
         return item.amountStr;
      case Columns::Address:
         return UiUtils::displayAddress(item.mainAddress);
      case Columns::RbfFlag:
         if (!item.confirmations) {
            if (item.txEntry.isRBF) {
               return tr("RBF");
            }
            else if (item.isCPFP) {
               return tr("CPFP");
            }
         }
         break;
      case Columns::TxHash:
         return QString::fromStdString(item.txEntry.txHash.toHexStr(true));
/*      case Columns::MissedBlocks:
         return item.confirmations < 6 ? 0 : QVariant();*/
      default:
         return QVariant();
      }
   }
   else if (role == WalletRole) {
      return qVariantFromValue(static_cast<void*>(item.wallet.get()));
   }
   else if (role == SortRole) {
      switch(col) {
      case Columns::Date:        return item.txEntry.txTime;
      case Columns::Status:      return item.confirmations;
      case Columns::Wallet:      return item.walletName;
      case Columns::SendReceive: return (int)item.direction;
      case Columns::Comment:     return item.comment;
      case Columns::Amount:      return QVariant::fromValue<double>(qAbs(item.amount));
      case Columns::Address:     return item.mainAddress;
      default:    return QVariant();
      }
   }
   else if (role == Qt::TextColorRole) {
      switch (col) {
         case Columns::Address:
         case Columns::Wallet:
            return colorGray_;

         case Columns::Status:
         {
            if (item.confirmations == 0) {
               return colorRed_;
            }
            else if (item.confirmations < 6) {
               return colorYellow_;
            }
            else {
               return colorGreen_;
            }
         }

         default:
            if (!item.isValid) {
               return colorInvalid_;
            }
            else {
               return QVariant();
            }
      }
   }
   else if (role == Qt::FontRole) {
      bool boldFont = false;
      if (col == Columns::Amount) {
         boldFont = true;
      }
      else if ((col == Columns::Status) && (item.confirmations < 6)) {
         boldFont = true;
      }
      if (boldFont) {
         return fontBold_;
      }
   }
   else if (role == FilterRole) {
      switch (col)
      {
         case Columns::Date:        return item.txEntry.txTime;
         case Columns::Wallet:      return item.walletID;
         case Columns::SendReceive: return item.direction;
         default:    return QVariant();
      }
   }

   return QVariant();
}

QVariant TransactionsViewModel::headerData(int section, Qt::Orientation orientation, int role) const
{
   if ((role == Qt::DisplayRole) && (orientation == Qt::Horizontal)) {
      switch(static_cast<Columns>(section)) {
      case Columns::Date:           return tr("Date");
      case Columns::Status:         return tr("Confirmations");
      case Columns::Wallet:         return tr("Wallet");
      case Columns::SendReceive:    return tr("Type");
      case Columns::Comment:        return tr("Comment");
      case Columns::Address:        return tr("Address");
      case Columns::Amount:         return tr("Amount");
      case Columns::RbfFlag:        return tr("Flag");
      case Columns::TxHash:           return tr("Hash");
//      case Columns::MissedBlocks:   return tr("Missed Blocks");
      default:    return QVariant();
      }
   }
   return QVariant();
}

void TransactionsViewModel::refresh()
{
   updatePage();
}

void TransactionsViewModel::updatePage()
{
//   walletsManager_->getNewTransactions();
   if (allWallets_) {
      loadAllWallets();
   }
}

void TransactionsViewModel::clear()
{
   stopped_ = true;
   beginResetModel();
   {
      QMutexLocker locker(&updateMutex_);
      currentPage_.clear();
      currentKeys_.clear();
   }
   endResetModel();
   stopped_ = false;
}

void TransactionsViewModel::onArmoryStateChanged(ArmoryConnection::State state)
{
   if (state == ArmoryConnection::State::Offline) {
      clear();
   }
   else if ((state == ArmoryConnection::State::Ready) && currentPage_.empty()) {
      loadLedgerEntries();
   }
}

TransactionsViewItem TransactionsViewModel::itemFromTransaction(const bs::TXEntry &entry)
{
   TransactionsViewItem item;
   item.txEntry = entry;
   item.displayDateTime = UiUtils::displayDateTime(entry.txTime);
   item.walletID = QString::fromStdString(entry.id);
   item.wallet = walletsManager_->GetWalletById(entry.id);
   if (!item.wallet && defaultWallet_) {
      item.wallet = defaultWallet_;
   }

   item.confirmations = armory_->getConfirmationsNumber(entry.blockNum);
   if (item.wallet) {
      item.walletName = QString::fromStdString(item.wallet->GetWalletName());
   }
   item.isValid = item.wallet ? item.wallet->isTxValid(entry.txHash) : false;
   item.initialized = false;
   return item;
}

static std::string mkTxKey(const BinaryData &txHash, const std::string &id)
{
   return txHash.toBinStr() + id;
}
static std::string mkTxKey(const bs::TXEntry &item)
{
   return mkTxKey(item.txHash, item.id);
}
static std::string mkTxKey(const TransactionsViewItem &item)
{
   return mkTxKey(item.txEntry);
}

bool TransactionsViewModel::txKeyExists(const std::string &key)
{
   return (currentKeys_.find(key) != currentKeys_.end());
}

void TransactionsViewModel::onNewTransactions(std::vector<bs::TXEntry> allPages)
{
   insertNewTransactions(allPages);
   updateBlockHeight(allPages);
}

void TransactionsViewModel::insertNewTransactions(const std::vector<bs::TXEntry> &page)
{
   if (!initialLoadCompleted_) {
      return;
   }
   TransactionItems newItems;
   newItems.reserve(page.size());
   const auto &settlWallet = walletsManager_->GetSettlementWallet();

   for (const auto entry : page) {
      if (settlWallet && settlWallet->isTempWalletId(entry.id)) {
         continue;
      }
      const auto item = itemFromTransaction(entry);
/*      if (!item.isValid) {
         continue;
      }*/
      const auto txKey = mkTxKey(item);
      {
         QMutexLocker locker(&updateMutex_);
         if (txKeyExists(txKey)) {
            continue;
         }
         currentKeys_.insert(txKey);
      }
      newItems.push_back(item);
   }

   if (!newItems.empty()) {
      addCommand({Command::Type::Add, newItems});
   }
}

void TransactionsViewModel::timerCmd()
{
   CommandQueue tempQueue;
   {
      QMutexLocker lock(&cmdMutex_);
      tempQueue.swap(cmdQueue_);
   }
   for (const auto &cmd : tempQueue) {
      executeCommand(cmd);
   }
}

void TransactionsViewModel::executeCommand(const Command &cmd)
{
   switch (cmd.type) {
   case Command::Type::Add:
      onNewItems(cmd.items);
      break;
   case Command::Type::Delete:
      onItemsDeleted(cmd.items);
      break;
   case Command::Type::Confirm:
      onItemsConfirmed(cmd.items);
      break;
   case Command::Type::Update:
      break;
   }
}

void TransactionsViewModel::addCommand(const Command &cmd)
{
   QMutexLocker lock(&cmdMutex_);
   cmdQueue_.emplace_back(cmd);
}

void TransactionsViewModel::updateBlockHeight(const std::vector<bs::TXEntry> &page)
{
   std::unordered_map<std::string, bs::TXEntry> newData;
   for (const auto &item : page) {
      newData[mkTxKey(item)] = item;
   }
   TransactionItems firstConfItems;
   {
      QMutexLocker locker(&updateMutex_);
      if (currentPage_.empty()) {
         return;
      }
      for (size_t i = 0; i < currentPage_.size(); i++) {
         auto &item = currentPage_[i];
         const auto itPage = newData.find(mkTxKey(item));
         uint32_t newBlockNum = UINT32_MAX;
         if (itPage != newData.end()) {
            newBlockNum = itPage->second.blockNum;
            if (item.wallet) {
               item.isValid = item.wallet->isTxValid(itPage->second.txHash);
            }
            if (item.txEntry.value != itPage->second.value) {
               item.txEntry = itPage->second;
               item.amountStr.clear();
               item.calcAmount(walletsManager_);
            }
         }
         if (newBlockNum != UINT32_MAX) {
            item.confirmations = armory_->getConfirmationsNumber(newBlockNum);
            if (item.confirmations == 1) {
               firstConfItems.push_back(item);
            }
         }
      }
   }
   emit dataChanged(index(0, static_cast<int>(Columns::Amount))
   , index(currentPage_.size() - 1, static_cast<int>(Columns::Status)));

   if (!firstConfItems.empty()) {
      addCommand({ Command::Type::Confirm, firstConfItems });
   }
}

void TransactionsViewModel::loadLedgerEntries()
{
   if (!initialLoadCompleted_) {
      return;
   }
   initialLoadCompleted_ = false;
   const auto &cbPageCount = [this](uint64_t pageCnt) {
      for (uint32_t pageId = 0; pageId < pageCnt; ++pageId) {
         const auto &cbLedger = [this, pageId, pageCnt](std::vector<ClientClasses::LedgerEntry> entries) {
            rawData_[pageId] = bs::convertTXEntries(entries);
            if (rawData_.size() >= pageCnt) {
               ledgerToTxData();
            }
         };
         ledgerDelegate_.getHistoryPage(pageId, cbLedger);
      }
   };
   ledgerDelegate_.getPageCount(cbPageCount);
}

void TransactionsViewModel::ledgerToTxData()
{
   size_t count = 0;
   const unsigned int iStart = currentPage_.size();
   std::vector<bs::TXEntry> updatedEntries;
   beginResetModel();
   {
      QMutexLocker locker(&updateMutex_);
      for (const auto &le : rawData_) {
         for (const auto &led : le.second) {
            const auto item = itemFromTransaction(led);
/*            if (!item.isValid) {
               continue;
            }*/
            const auto txKey = mkTxKey(item);
            if (txKeyExists(txKey)) {
               updatedEntries.emplace_back(std::move(led));
            }
            else {
               count++;
               currentKeys_.insert(txKey);
               currentPage_.emplace_back(std::move(item));
            }
         }
      }
   }
   endResetModel();
   rawData_.clear();

   if (!updatedEntries.empty()) {
      updateBlockHeight(updatedEntries);
   }
   initialLoadCompleted_ = true;

   if (count) {
      if (updatedEntries.empty()) {
         emit dataLoaded(currentPage_.size());
      }
      loadTransactionDetails(iStart, count);
   }
}

void TransactionsViewModel::onNewItems(TransactionItems items)
{
   unsigned int curLastIdx = currentPage_.size();
   beginInsertRows(QModelIndex(), curLastIdx, curLastIdx + items.size() - 1);
   {
      QMutexLocker locker(&updateMutex_);
      currentPage_.insert(currentPage_.end(), items.begin(), items.end());
   }
   endInsertRows();

   loadTransactionDetails(curLastIdx, items.size());
}

int TransactionsViewModel::getItemIndex(const TransactionsViewItem &item) const
{
   QMutexLocker locker(&updateMutex_);
   for (int i = 0; i < currentPage_.size(); i++) {
      const auto &curItem = currentPage_[i];
      if (mkTxKey(item) == mkTxKey(curItem)) {
         return i;
      }
   }
   return -1;
}

void TransactionsViewModel::onItemsDeleted(TransactionItems items)
{
   for (const auto &item : items) {
      const int idx = getItemIndex(item);
      if (idx < 0) {
         continue;
      }
      beginRemoveRows(QModelIndex(), idx, idx);
      {
         QMutexLocker locker(&updateMutex_);
         currentPage_.erase(currentPage_.begin() + idx);
      }
      endRemoveRows();
   }
}

void TransactionsViewModel::onRowUpdated(int row, const TransactionsViewItem &item, int c1, int c2)
{
   const auto &itemKey = mkTxKey(item);
   for (int i = qMax<int>(0, row - 5); i < qMin<int>(currentPage_.size(), row + 5); i++) {
      if (mkTxKey(currentPage_[i]) == itemKey) {
         currentPage_[i] = item;
         emit dataChanged(index(i, c1), index(i, c2));
         break;
      }
   }
}

void TransactionsViewModel::onItemsConfirmed(TransactionItems items)
{
   TransactionItems doubleSpendItems;
   {
      QMutexLocker locker(&updateMutex_);
      for (size_t i = 0; i < currentPage_.size(); i++) {
         const auto &curItem = currentPage_[i];
         if (curItem.confirmations) {
            continue;
         }
         for (const auto &item : items) {
            if (!item.tx.isInitialized()) {
               continue;
            }
            if (curItem.containsInputsFrom(item.tx)) {
               doubleSpendItems.push_back(curItem);
            }
         }
      }
   }
   if (!doubleSpendItems.empty()) {
      addCommand({Command::Type::Delete, doubleSpendItems});
   }
}

bool TransactionsViewModel::isTransactionVerified(int transactionRow) const
{
   QMutexLocker locker(&updateMutex_);
   const auto& item = currentPage_[transactionRow];
   return armory_->isTransactionVerified(item.txEntry.blockNum);
}

TransactionsViewItem TransactionsViewModel::getItem(int row) const
{
   QMutexLocker locker(&updateMutex_);
   if ((row < 0) || (row >= currentPage_.size())) {
      return {};
   }
   return currentPage_[row];
}

void TransactionsViewModel::loadTransactionDetails(unsigned int iStart, size_t count)
{
   const auto pageSize = currentPage_.size();

   std::set<int> uninitedItems;
   {
      QMutexLocker locker(&updateMutex_);
      for (unsigned int i = iStart; i < qMin<unsigned int>(pageSize, iStart + count); i++) {
         if (stopped_) {
            break;
         }
         auto &item = currentPage_[i];
         if (!item.initialized) {
            updateTransactionDetails(item, i);
         }
      }
   }
}

void TransactionsViewModel::updateTransactionDetails(TransactionsViewItem &item, int index)
{
   const auto &cbInited = [this, index](const TransactionsViewItem *itemPtr) {
      onRowUpdated(index, *itemPtr, static_cast<int>(Columns::SendReceive), static_cast<int>(Columns::Amount));
   };
   item.initialize(armory_, walletsManager_, cbInited);
}


void TransactionsViewItem::initialize(const std::shared_ptr<ArmoryConnection> &armory
   , const std::shared_ptr<WalletsManager> &walletsMgr, std::function<void(const TransactionsViewItem *)> cb)
{
   const auto &cbInit = [this, walletsMgr, cb] {
      if (amountStr.isEmpty() && txHashes.empty()) {
         calcAmount(walletsMgr);
      }
      if (!dirStr.isEmpty() && !mainAddress.isEmpty() && !amountStr.isEmpty()) {
         initialized = true;
         cb(this);
      }
   };
   const auto &cbTXs = [this, cbInit](std::vector<Tx> txs) {
      for (const auto &tx : txs) {
         const auto &txHash = tx.getThisHash();
         txHashes.erase(txHash);
         txIns[txHash] = tx;
         if (txHashes.empty()) {
            cbInit();
         }
      }
   };
   const auto &cbDir = [this, cbInit](bs::Transaction::Direction dir) {
      direction = dir;
      dirStr = QObject::tr(bs::Transaction::toStringDir(dir));
      cbInit();
   };
   const auto &cbMainAddr = [this, cbInit](QString mainAddr) {
      mainAddress = mainAddr;
      cbInit();
   };
   const auto &cbTX = [this, armory, walletsMgr, cbTXs, cbInit, cbDir, cbMainAddr](Tx newTx) {
      if (!newTx.isInitialized()) {
         return;
      }
      if (comment.isEmpty()) {
         comment = wallet ? QString::fromStdString(wallet->GetTransactionComment(txEntry.txHash))
            : QString();
         const auto endLineIndex = comment.indexOf(QLatin1Char('\n'));
         if (endLineIndex != -1) {
            comment = comment.left(endLineIndex) + QLatin1String("...");
         }
      }

      if (!tx.isInitialized()) {
         tx = std::move(newTx);
         std::set<BinaryData> txHashSet;
         for (size_t i = 0; i < tx.getNumTxIn(); i++) {
            TxIn in = tx.getTxInCopy(i);
            OutPoint op = in.getOutPoint();
            if (txIns.find(op.getTxHash()) == txIns.end()) {
               txHashSet.insert(op.getTxHash());
            }
         }
         if (txHashSet.empty()) {
            cbInit();
         }
         else {
            txHashes = txHashSet;
            armory->getTXsByHash(txHashSet, cbTXs);
         }
      }

      if (dirStr.isEmpty()) {
         walletsMgr->GetTransactionDirection(tx, wallet, cbDir);
      }
      if (mainAddress.isEmpty()) {
         walletsMgr->GetTransactionMainAddress(tx, wallet, (amount > 0), cbMainAddr);
      }
   };

   if (initialized) {
      cb(this);
   }
   else {
      if (tx.isInitialized()) {
         cbTX(tx);
      }
      else {
         armory->getTxByHash(txEntry.txHash, cbTX);
      }
   }
}

static bool isSpecialWallet(const std::shared_ptr<bs::Wallet> &wallet)
{
   if (!wallet) {
      return false;
   }
   switch (wallet->GetType()) {
   case bs::wallet::Type::Settlement:
   case bs::wallet::Type::ColorCoin:
      return true;
   default: break;
   }
   return false;
}

void TransactionsViewItem::calcAmount(const std::shared_ptr<WalletsManager> &walletsManager)
{
   if (wallet) {
      if (!tx.isInitialized()) {
         return;
      }
      bool hasSpecialAddr = false;
      int64_t outputVal = 0;
      for (size_t i = 0; i < tx.getNumTxOut(); ++i) {
         TxOut out = tx.getTxOutCopy(i);
         if (txEntry.isChainedZC && !hasSpecialAddr) {
            const auto addr = bs::Address::fromTxOut(out);
            hasSpecialAddr = isSpecialWallet(walletsManager->GetWalletByAddress(addr.id()));
         }
         outputVal += out.getValue();
      }

      int64_t inputVal = 0;
      for (size_t i = 0; i < tx.getNumTxIn(); i++) {
         TxIn in = tx.getTxInCopy(i);
         OutPoint op = in.getOutPoint();
         const auto &prevTx = txIns[op.getTxHash()];
         if (prevTx.isInitialized()) {
            TxOut prevOut = prevTx.getTxOutCopy(op.getTxOutIndex());
            inputVal += prevOut.getValue();
            if (txEntry.isChainedZC && !hasSpecialAddr) {
               const auto addr = bs::Address::fromTxOut(prevTx.getTxOutCopy(op.getTxOutIndex()));
               hasSpecialAddr = isSpecialWallet(walletsManager->GetWalletByAddress(addr.id()));
            }
         }
      }
      auto value = txEntry.value;
      const auto fee = (wallet->GetType() == bs::wallet::Type::ColorCoin) || (value > 0) ? 0 : (outputVal - inputVal);
      value -= fee;
      amount = wallet->GetTxBalance(value);
      amountStr = wallet->displayTxValue(value);

      if (txEntry.isChainedZC && (wallet->GetType() == bs::wallet::Type::Bitcoin) && !hasSpecialAddr) {
         isCPFP = true;
      }
   }
   else {
      amount = txEntry.value / BTCNumericTypes::BalanceDivider;
      amountStr = UiUtils::displayAmount(amount);
   }
}

bool TransactionsViewItem::containsInputsFrom(const Tx &inTx) const
{
   const bs::TxChecker checker(tx);

   for (size_t i = 0; i < inTx.getNumTxIn(); i++) {
      TxIn in = inTx.getTxInCopy((int)i);
      if (!in.isInitialized()) {
         continue;
      }
      OutPoint op = in.getOutPoint();
      if (checker.hasInput(op.getTxHash())) {
         return true;
      }
   }
   return false;
}
