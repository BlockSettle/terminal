#include "TransactionsViewModel.h"

#include "ArmoryConnection.h"
#include "CheckRecipSigner.h"
#include "UiUtils.h"
#include "WalletsManager.h"

#include <QDateTime>
#include <QMutexLocker>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>


TXNode::TXNode()
{
   init();
}

TXNode::TXNode(const std::shared_ptr<TransactionsViewItem> &item)
   : item_(item)
{
   init();
}

void TXNode::init()
{
   fontBold_.setBold(true);
   colorGray_ = Qt::darkGray, colorRed_ = Qt::red, colorYellow_ = Qt::darkYellow;
   colorGreen_ = Qt::darkGreen, colorInvalid_ = Qt::red;
}

void TXNode::clear()
{
   qDeleteAll(children_);
   children_.clear();
}

TXNode *TXNode::child(int index) const
{
   return ((index >= nbChildren()) || (index < 0)) ? nullptr : children_[index];
}

QVariant TXNode::data(int column, int role) const
{
   if (!item_) {
      return {};
   }
   const auto col = static_cast<TransactionsViewModel::Columns>(column);
   if (role == Qt::DisplayRole) {
      switch (col) {
      case TransactionsViewModel::Columns::Date:
         return item_->displayDateTime;
      case TransactionsViewModel::Columns::Status:
         return QObject::tr("   %1").arg(item_->confirmations);
      case TransactionsViewModel::Columns::Wallet:
         return item_->walletName;
      case TransactionsViewModel::Columns::SendReceive:
         return item_->dirStr;
      case TransactionsViewModel::Columns::Comment:
         return item_->comment;
      case TransactionsViewModel::Columns::Amount:
         return item_->amountStr;
      case TransactionsViewModel::Columns::Address:
         return UiUtils::displayAddress(item_->mainAddress);
      case TransactionsViewModel::Columns::RbfFlag:
         if (!item_->confirmations) {
            if (item_->txEntry.isRBF) {
               return QObject::tr("RBF");
            } else if (item_->isCPFP) {
               return QObject::tr("CPFP");
            }
         }
         break;
      case TransactionsViewModel::Columns::TxHash:
         return QString::fromStdString(item_->txEntry.txHash.toHexStr(true));
         /*      case Columns::MissedBlocks:
                  return item.confirmations < 6 ? 0 : QVariant();*/
      default:
         return QVariant();
      }
   } else if (role == TransactionsViewModel::WalletRole) {
      return qVariantFromValue(static_cast<void*>(item_->wallet.get()));
   } else if (role == TransactionsViewModel::SortRole) {
      switch (col) {
      case TransactionsViewModel::Columns::Date:        return item_->txEntry.txTime;
      case TransactionsViewModel::Columns::Status:      return item_->confirmations;
      case TransactionsViewModel::Columns::Wallet:      return item_->walletName;
      case TransactionsViewModel::Columns::SendReceive: return (int)item_->direction;
      case TransactionsViewModel::Columns::Comment:     return item_->comment;
      case TransactionsViewModel::Columns::Amount:      return QVariant::fromValue<double>(qAbs(item_->amount));
      case TransactionsViewModel::Columns::Address:     return item_->mainAddress;
      default:    return QVariant();
      }
   } else if (role == Qt::TextColorRole) {
      switch (col) {
      case TransactionsViewModel::Columns::Address:
      case TransactionsViewModel::Columns::Wallet:
         return colorGray_;

      case TransactionsViewModel::Columns::Status:
      {
         if (item_->confirmations == 0) {
            return colorRed_;
         } else if (item_->confirmations < 6) {
            return colorYellow_;
         } else {
            return colorGreen_;
         }
      }

      default:
         if (!item_->isValid) {
            return colorInvalid_;
         } else {
            return QVariant();
         }
      }
   } else if (role == Qt::FontRole) {
      bool boldFont = false;
      if (col == TransactionsViewModel::Columns::Amount) {
         boldFont = true;
      } else if ((col == TransactionsViewModel::Columns::Status) && (item_->confirmations < 6)) {
         boldFont = true;
      }
      if (boldFont) {
         return fontBold_;
      }
   } else if (role == TransactionsViewModel::FilterRole) {
      switch (col)
      {
      case TransactionsViewModel::Columns::Date:        return item_->txEntry.txTime;
      case TransactionsViewModel::Columns::Wallet:      return item_->walletID;
      case TransactionsViewModel::Columns::SendReceive: return item_->direction;
      case TransactionsViewModel::Columns::Address:     return item_->mainAddress;
      case TransactionsViewModel::Columns::Comment:     return item_->comment;
      default:    return QVariant();
      }
   }

   return QVariant();
}

void TXNode::add(TXNode *child)
{
   child->row_ = nbChildren();
   child->parent_ = this;
   children_.append(child);
}

void TXNode::forEach(const std::function<void(const std::shared_ptr<TransactionsViewItem> &)> &cb)
{
   if (item_) {
      cb(item_);
   }
   for (auto child : children_) {
      child->forEach(cb);
   }
}

TXNode *TXNode::find(const std::string &id) const
{
   if (item_ && (item_->id() == id)) {
      return (TXNode *)this;
   }
   for (auto child : children_) {
      auto result = child->find(id);
      if (result) {
         return result;
      }
   }
   return nullptr;
}


TransactionsViewModel::TransactionsViewModel(const std::shared_ptr<ArmoryConnection> &armory
                         , const std::shared_ptr<WalletsManager> &walletsManager
                             , const AsyncClient::LedgerDelegate &ledgerDelegate
                                 , const std::shared_ptr<spdlog::logger> &logger
                                             , QObject* parent
                                    , const std::shared_ptr<bs::Wallet> &defWlt)
   : QAbstractTableModel(parent)
   , armory_(armory)
   , ledgerDelegate_(ledgerDelegate)
   , walletsManager_(walletsManager)
   , defaultWallet_(defWlt)
   , logger_(logger)
   , allWallets_(false)
{
   init();
   QtConcurrent::run(this, &TransactionsViewModel::loadLedgerEntries);
}

TransactionsViewModel::TransactionsViewModel(const std::shared_ptr<ArmoryConnection> &armory
                         , const std::shared_ptr<WalletsManager> &walletsManager
                                 , const std::shared_ptr<spdlog::logger> &logger
                                             , QObject* parent)
   : QAbstractTableModel(parent)
   , armory_(armory)
   , walletsManager_(walletsManager)
   , logger_(logger)
   , allWallets_(true)
{
   init();
}

void TransactionsViewModel::init()
{
   stopped_ = false;
   initialLoadCompleted_ = true;
   qRegisterMetaType<TransactionsViewItem>();
   qRegisterMetaType<TransactionItems>();

   rootNode_ = new TXNode;

   if (armory_) {
      connect(armory_.get(), SIGNAL(stateChanged(ArmoryConnection::State)), this, SLOT(onArmoryStateChanged(ArmoryConnection::State)), Qt::QueuedConnection);
      connect(armory_.get(), &ArmoryConnection::newBlock, this, &TransactionsViewModel::updatePage, Qt::QueuedConnection);
   }
   connect(walletsManager_.get(), &WalletsManager::walletChanged, this, &TransactionsViewModel::refresh, Qt::QueuedConnection);
   connect(walletsManager_.get(), &WalletsManager::walletImportFinished, [this](const std::string &) { refresh(); });
   connect(walletsManager_.get(), &WalletsManager::walletsReady, this, &TransactionsViewModel::updatePage, Qt::QueuedConnection);
   connect(walletsManager_.get(), &WalletsManager::newTransactions, this, &TransactionsViewModel::onNewTransactions, Qt::QueuedConnection);
}

TransactionsViewModel::~TransactionsViewModel()
{
   stopped_ = true;
   delete rootNode_;
}

void TransactionsViewModel::loadAllWallets()
{
   const auto &cbWalletsLD = [this](AsyncClient::LedgerDelegate delegate) {
      ledgerDelegate_ = delegate;
      QtConcurrent::run(this, &TransactionsViewModel::loadLedgerEntries);
   };
   armory_->getWalletsLedgerDelegate(cbWalletsLD);
}

int TransactionsViewModel::columnCount(const QModelIndex &) const
{
   return static_cast<int>(Columns::last);
}

TXNode *TransactionsViewModel::getNode(const QModelIndex &index) const
{
   if (!index.isValid()) {
      return rootNode_;
   }
   return static_cast<TXNode *>(index.internalPointer());
}

int TransactionsViewModel::rowCount(const QModelIndex &parent) const
{
   if (parent.isValid()) {
      return 0;
   }
   return static_cast<int>(getNode(parent)->nbChildren());
}

QModelIndex TransactionsViewModel::index(int row, int column, const QModelIndex &parent) const
{
   if (!hasIndex(row, column, parent)) {
      return QModelIndex();
   }

   auto node = getNode(parent);
   auto child = node->child(row);
   if (child == nullptr) {
      return QModelIndex();
   }
   return createIndex(row, column, static_cast<void*>(child));
}

QModelIndex TransactionsViewModel::parent(const QModelIndex &child) const
{
   if (!child.isValid()) {
      return QModelIndex();
   }

   auto node = getNode(child);
   auto parentNode = (node == nullptr) ? nullptr : node->parent();
   if ((parentNode == nullptr) || (parentNode == rootNode_)) {
      return QModelIndex();
   }
   return createIndex(parentNode->row(), 0, static_cast<void*>(parentNode));
}

bool TransactionsViewModel::hasChildren(const QModelIndex& parent) const
{
   return getNode(parent)->hasChildren();
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

   return getNode(index)->data(index.column(), role);
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
      rootNode_->clear();
      currentItems_.clear();
   }
   endResetModel();
   stopped_ = false;
}

void TransactionsViewModel::onArmoryStateChanged(ArmoryConnection::State state)
{
   if (state == ArmoryConnection::State::Offline) {
      clear();
   }
   else if ((state == ArmoryConnection::State::Ready) && !rootNode_->hasChildren()) {
      loadLedgerEntries();
   }
}

std::shared_ptr<TransactionsViewItem> TransactionsViewModel::itemFromTransaction(const bs::TXEntry &entry)
{
   auto item = std::make_shared<TransactionsViewItem>();
   item->txEntry = entry;
   item->displayDateTime = UiUtils::displayDateTime(entry.txTime);
   item->walletID = QString::fromStdString(entry.id);
   item->wallet = walletsManager_->GetWalletById(entry.id);
   if (!item->wallet && defaultWallet_) {
      item->wallet = defaultWallet_;
   }

   item->confirmations = armory_->getConfirmationsNumber(entry.blockNum);
   if (item->wallet) {
      item->walletName = QString::fromStdString(item->wallet->GetWalletName());
   }
   item->isValid = item->wallet ? item->wallet->isTxValid(entry.txHash) : false;
   item->initialized = false;
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

bool TransactionsViewModel::txKeyExists(const std::string &key)
{
   return (currentItems_.find(key) != currentItems_.end());
}

void TransactionsViewModel::onNewTransactions(std::vector<bs::TXEntry> page)
{
   pendingNewItems_.insert(pendingNewItems_.end(), page.begin(), page.end());
   if (!initialLoadCompleted_) {
      return;
   }
   initialLoadCompleted_ = false;
   updateTransactionsPage(pendingNewItems_);
   initialLoadCompleted_ = true;
}

std::pair<size_t, size_t> TransactionsViewModel::updateTransactionsPage(const std::vector<bs::TXEntry> &page)
{
   auto newItems = std::make_shared<std::unordered_map<std::string, std::shared_ptr<TransactionsViewItem>>>();
   auto newTxKeys = std::make_shared<std::unordered_set<std::string>>();
   std::vector<std::shared_ptr<TransactionsViewItem>> updatedItems;

   for (const auto &entry : page) {
      const auto item = itemFromTransaction(entry);
      if (!item->wallet) {
         continue;
      }
      {
         QMutexLocker locker(&updateMutex_);
         if (txKeyExists(item->id())) {
            updatedItems.push_back(item);
            continue;
         }
         currentItems_[item->id()] = item;
      }
      newTxKeys->insert(item->id());
      (*newItems)[item->id()] = item;
   }
   pendingNewItems_.clear();

   if (!newItems->empty()) {
      for (auto &item : *newItems) {
         const auto &cbInited = [this, newItems, newTxKeys](const TransactionsViewItem *itemPtr) {
            if (!itemPtr || !itemPtr->initialized) {
               logger_->error("item is not inited");
               return;
            }
            newTxKeys->erase(itemPtr->id());
            if (newTxKeys->empty()) {
               TransactionItems items;
               items.reserve(newItems->size());
               for (const auto &item : *newItems) {
                  items.push_back(*item.second);
               }
               QMetaObject::invokeMethod(this, [this, items] { onNewItems(items); });
            }
         };
         updateTransactionDetails(item.second, cbInited);
      }
   }
   if (!updatedItems.empty()) {
      updateBlockHeight(updatedItems);
   }
   return { newItems->size(), updatedItems.size() };
}

void TransactionsViewModel::updateBlockHeight(const std::vector<std::shared_ptr<TransactionsViewItem>> &updItems)
{
   {
      if (!rootNode_->hasChildren()) {
         return;
      }

      for (const auto &updItem : updItems) {
         const auto &itItem = currentItems_.find(updItem->id());
         if (itItem == currentItems_.end()) {
            continue;
         }
         const auto &item = itItem->second;
         uint32_t newBlockNum = UINT32_MAX;
         newBlockNum = updItem->txEntry.blockNum;
         if (item->wallet) {
            item->isValid = item->wallet->isTxValid(updItem->txEntry.txHash);
         }
         if (item->txEntry.value != updItem->txEntry.value) {
            item->txEntry = updItem->txEntry;
            item->amountStr.clear();
            item->calcAmount(walletsManager_);
         }
         if (newBlockNum != UINT32_MAX) {
            item->confirmations = armory_->getConfirmationsNumber(newBlockNum);
            item->txEntry.blockNum = newBlockNum;
         }
      }
   }
   emit dataChanged(index(0, static_cast<int>(Columns::Amount))
   , index(rootNode_->nbChildren() - 1, static_cast<int>(Columns::Status)));
}

void TransactionsViewModel::loadLedgerEntries()
{
   if (!initialLoadCompleted_) {
      return;
   }
   initialLoadCompleted_ = false;
   const auto &cbPageCount = [this](ReturnMessage<uint64_t> pageCnt)->void {
      try {
         auto inPageCnt = pageCnt.get();
         for (uint64_t pageId = 0; pageId < inPageCnt; ++pageId) {
            const auto &cbLedger = [this, pageId, inPageCnt]
               (ReturnMessage<std::vector<ClientClasses::LedgerEntry>> entries)->void {
               try {
                 auto le = entries.get();
                 rawData_[pageId] = bs::convertTXEntries(le);
               }
               catch (exception& e) {
                  logger_->error("[TransactionsViewModel::loadLedgerEntries] " \
                     "Return data error (getPageCount) - {}", e.what());
               }

               if (rawData_.size() >= inPageCnt) {
                  ledgerToTxData();
               }
            };
            ledgerDelegate_.getHistoryPage(pageId, cbLedger);
         }
      }
      catch (exception& e) {
         logger_->error("[TransactionsViewModel::loadLedgerEntries] Return " \
            "data error (getPageCount) - {}", e.what());
      }
   };

   ledgerDelegate_.getPageCount(cbPageCount);
}

void TransactionsViewModel::ledgerToTxData()
{
   size_t newItems = 0;
   size_t updItems = 0;

   for (const auto &le : rawData_) {
      const auto &rc = updateTransactionsPage(le.second);
      newItems += rc.first;
      updItems += rc.second;
   }
   initialLoadCompleted_ = true;

   if (newItems && !updItems) {
      emit dataLoaded(newItems);
   }
}

void TransactionsViewModel::onNewItems(TransactionItems items)
{
   unsigned int curLastIdx = rootNode_->nbChildren();
   beginInsertRows(QModelIndex(), curLastIdx, curLastIdx + items.size() - 1);
   {
      QMutexLocker locker(&updateMutex_);
      for (const auto &item : items) {
         const auto &newItem = std::make_shared<TransactionsViewItem>(item);
         currentItems_[newItem->id()] = newItem;
         rootNode_->add(new TXNode(newItem));
      }
   }
   endInsertRows();
}

TransactionsViewItem TransactionsViewModel::getItem(int row) const
{
   QMutexLocker locker(&updateMutex_);
   if ((row < 0) || (row >= (int)rootNode_->nbChildren())) {
      return {};
   }
   return *(rootNode_->child(row)->item());
}

void TransactionsViewModel::updateTransactionDetails(const std::shared_ptr<TransactionsViewItem> &item
   , const std::function<void(const TransactionsViewItem *itemPtr)> &cb)
{
   const auto &cbInited = [this, cb](const TransactionsViewItem *itemPtr) {
      if (cb) {
         cb(itemPtr);
      }
      else {
         logger_->warn("[TransactionsViewModel::updateTransactionDetails] missing callback");
      }
   };
   item->initialize(armory_, walletsManager_, cbInited);
}


void TransactionsViewItem::initialize(const std::shared_ptr<ArmoryConnection> &armory
   , const std::shared_ptr<WalletsManager> &walletsMgr, std::function<void(const TransactionsViewItem *)> userCB)
{
   const auto cbCheckIfInitializationCompleted = [this, userCB]()
   {
      if (!dirStr.isEmpty() && !mainAddress.isEmpty() && !amountStr.isEmpty()) {
         initialized = true;
         userCB(this);
      }
   };

   const auto cbMainAddr = [this, cbCheckIfInitializationCompleted](QString mainAddr) {
      mainAddress = mainAddr;
      cbCheckIfInitializationCompleted();
   };

   const auto cbInit = [this, walletsMgr, cbMainAddr, cbCheckIfInitializationCompleted] {
      if (amountStr.isEmpty() && txHashes.empty()) {
         calcAmount(walletsMgr);
         if (mainAddress.isEmpty()) {
            walletsMgr->GetTransactionMainAddress(tx, wallet, (amount > 0), cbMainAddr);
         }
      }
      cbCheckIfInitializationCompleted();
   };

   const auto cbTXs = [this, cbInit](std::vector<Tx> txs) {
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

   const auto cbTX = [this, armory, walletsMgr, cbTXs, cbInit, cbDir, cbMainAddr](Tx newTx) {
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
   };

   if (initialized) {
      userCB(this);
   } else {
      if (tx.isInitialized()) {
         cbTX(tx);
      } else {
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

std::string TransactionsViewItem::id() const
{
   if (id_.empty()) {
      id_ = mkTxKey(txEntry);
   }
   return id_;
}

bool TransactionsViewItem::isRBFeligible() const
{
   return ((confirmations == 0) && txEntry.isRBF
      && (wallet != nullptr && wallet->GetType() != bs::wallet::Type::Settlement)
      && (direction == bs::Transaction::Direction::Internal || direction == bs::Transaction::Direction::Sent));
}

bool TransactionsViewItem::isCPFPeligible() const
{
   return ((confirmations == 0) && (wallet != nullptr && wallet->GetType() != bs::wallet::Type::Settlement)
      && (direction == bs::Transaction::Direction::Internal || direction == bs::Transaction::Direction::Received));
}
