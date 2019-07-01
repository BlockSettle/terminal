#include "TransactionsViewModel.h"

#include "ArmoryConnection.h"
#include "CheckRecipSigner.h"
#include "UiUtils.h"
#include "Wallets/SyncWalletsManager.h"

#include <QApplication>
#include <QDateTime>
#include <QMutexLocker>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrentRun>


TXNode::TXNode()
{
   init();
}

TXNode::TXNode(const std::shared_ptr<TransactionsViewItem> &item, TXNode *parent)
   : item_(item), parent_(parent)
{
   init();
}

void TXNode::init()
{
   fontBold_.setBold(true);
   colorGray_ = Qt::darkGray, colorRed_ = Qt::red, colorYellow_ = Qt::darkYellow;
   colorGreen_ = Qt::darkGreen, colorInvalid_ = Qt::red;
}

void TXNode::clear(bool del)
{
   if (del) {
      qDeleteAll(children_);
   }
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
      case TransactionsViewModel::Columns::Flag:
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

void TXNode::del(int index)
{
   if (index >= children_.size()) {
      return;
   }
   children_.removeAt(index);
   for (int i = index; i < children_.size(); ++i) {
      children_[i]->row_--;
   }
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
   for (const auto &child : children_) {
      if (child->item()->id() == id) {
         return child;
      }
   }
   return nullptr;
}

unsigned int TXNode::level() const
{
   unsigned int result = 0;
   auto parent = parent_;
   while (parent) {
      parent = parent->parent_;
      result++;
   }
   return result;
}


TransactionsViewModel::TransactionsViewModel(const std::shared_ptr<ArmoryConnection> &armory
                         , const std::shared_ptr<bs::sync::WalletsManager> &walletsManager
                         , const std::shared_ptr<AsyncClient::LedgerDelegate> &ledgerDelegate
                         , const std::shared_ptr<spdlog::logger> &logger
                         , QObject* parent
                         , const std::shared_ptr<bs::sync::Wallet> &defWlt)
   : QAbstractItemModel(parent)
   , ArmoryCallbackTarget(armory.get())
   , logger_(logger)
   , ledgerDelegate_(ledgerDelegate)
   , walletsManager_(walletsManager)
   , defaultWallet_(defWlt)
   , allWallets_(false)
{
   init();
   QtConcurrent::run(this, &TransactionsViewModel::loadLedgerEntries);
}

TransactionsViewModel::TransactionsViewModel(const std::shared_ptr<ArmoryConnection> &armory
                         , const std::shared_ptr<bs::sync::WalletsManager> &walletsManager
                                 , const std::shared_ptr<spdlog::logger> &logger
                                             , QObject* parent)
   : QAbstractItemModel(parent)
   , ArmoryCallbackTarget(armory.get())
   , walletsManager_(walletsManager)
   , logger_(logger)
   , allWallets_(true)
{
   init();
}

void TransactionsViewModel::init()
{
   stopped_ = false;
   qRegisterMetaType<TransactionsViewItem>();
   qRegisterMetaType<TransactionItems>();

   rootNode_ = new TXNode;

   connect(walletsManager_.get(), &bs::sync::WalletsManager::walletChanged, this, &TransactionsViewModel::refresh, Qt::QueuedConnection);
   connect(walletsManager_.get(), &bs::sync::WalletsManager::walletDeleted, this, &TransactionsViewModel::onWalletDeleted, Qt::QueuedConnection);
   connect(walletsManager_.get(), &bs::sync::WalletsManager::walletImportFinished, this, &TransactionsViewModel::refresh, Qt::QueuedConnection);
   connect(walletsManager_.get(), &bs::sync::WalletsManager::walletsReady, this, &TransactionsViewModel::updatePage, Qt::QueuedConnection);
}

TransactionsViewModel::~TransactionsViewModel() noexcept
{
   stopped_ = true;
   delete rootNode_;
}

void TransactionsViewModel::onNewBlock(unsigned int)
{
   QMetaObject::invokeMethod(this, [this] { updatePage(); });
}

void TransactionsViewModel::loadAllWallets()
{
   const auto &cbWalletsLD = [this](const std::shared_ptr<AsyncClient::LedgerDelegate> &delegate) {
      if (!initialLoadCompleted_) {
         return;
      }
      ledgerDelegate_ = delegate;
      QtConcurrent::run(this, &TransactionsViewModel::loadLedgerEntries);
   };
   if (initialLoadCompleted_) {
      armory_->getWalletsLedgerDelegate(cbWalletsLD);
   }
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
   const auto &node = getNode(parent);
   if (!node) {
      logger_->debug("failed to get node for {}", parent.row());
      return 0;
   }
   return static_cast<int>(node->nbChildren());
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
   const auto &node = getNode(parent);
   if (!node) {
      logger_->debug("Node not found for {}", parent.row());
      return false;
   }
   return node->hasChildren();
}

QVariant TransactionsViewModel::data(const QModelIndex &index, int role) const
{
   const auto col = static_cast<Columns>(index.column());

   if (role == Qt::TextAlignmentRole) {
      switch (col) {
      case Columns::Amount:   return Qt::AlignRight;
      case Columns::Flag:     return Qt::AlignCenter;
      default:  break;
      }
      return {};
   }

   const auto &node = getNode(index);
   if (!node) {
      return {};
   }
   return node->data(index.column(), role);
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
      case Columns::Flag:           return tr("Flag");
      case Columns::TxHash:         return tr("Hash");
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

void TransactionsViewModel::onWalletDeleted(std::string)
{
   clear();
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
      oldestItem_ = {};
   }
   endResetModel();
   stopped_ = false;
}

void TransactionsViewModel::onStateChanged(ArmoryState state)
{
   QMetaObject::invokeMethod(this, [this, state] {
      if (state == ArmoryState::Offline) {
         ledgerDelegate_.reset();
         clear();
      } else if ((state == ArmoryState::Ready) && !rootNode_->hasChildren()) {
         loadAllWallets();
      }
   });
}

std::shared_ptr<TransactionsViewItem> TransactionsViewModel::itemFromTransaction(const bs::TXEntry &entry)
{
   auto item = std::make_shared<TransactionsViewItem>();
   item->txEntry = entry;
   item->displayDateTime = UiUtils::displayDateTime(entry.txTime);
   item->walletID = QString::fromStdString(entry.id);
   item->wallet = walletsManager_->getWalletById(entry.id);
   if (!item->wallet && defaultWallet_) {
      item->wallet = defaultWallet_;
      item->walletID = QString::fromStdString(defaultWallet_->walletId());
   }

   item->confirmations = armory_->getConfirmationsNumber(entry.blockNum);
   if (item->wallet) {
      item->walletName = QString::fromStdString(item->wallet->name());
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
   std::string id;
   id.reserve(item.id.size());
   for (const auto &c : item.id) {
      id.push_back(tolower(c));
   }
   return mkTxKey(item.txHash, id);
}

bool TransactionsViewModel::txKeyExists(const std::string &key)
{
   return (currentItems_.find(key) != currentItems_.end());
}

void TransactionsViewModel::onZCReceived(const std::vector<bs::TXEntry> &entries)
{
   QMetaObject::invokeMethod(this, [this, entries] { updateTransactionsPage(entries); });
}

void TransactionsViewModel::onZCInvalidated(const std::vector<bs::TXEntry> &entries)
{
   std::vector<int> delRows;
   std::vector<bs::TXEntry> children;
   {
      QMutexLocker locker(&updateMutex_);
      for (const auto &entry : entries) {
         const auto key = mkTxKey(entry);
         const auto node = rootNode_->find(key);
         if (node && (node->parent() == rootNode_) && !node->item()->confirmations) {
            delRows.push_back(node->row());
            currentItems_.erase(key);
            if (node->hasChildren()) { // handle race condition when node being deleted has confirmed children
               for (const auto &child : node->children()) {
                  currentItems_.erase(mkTxKey(child->item()->txEntry));
                  if (child->item()->confirmations) {
                     children.push_back(child->item()->txEntry);
                  }
               }
            }
         }
      }
   }
   if (!delRows.empty()) {
      onDelRows(delRows);
   }
   if (!children.empty()) {
      updateTransactionsPage(children);
   }
}

static bool isChildOf(TransactionPtr child, TransactionPtr parent)
{
   if (!child->initialized || !parent->initialized) {
      return false;
   }
   if (!parent->parentId.isNull() && !child->groupId.isNull()) {
      if (child->groupId == parent->parentId) {
         return true;
      }
   }
   if ((!child->confirmations && child->txEntry.isRBF && !parent->confirmations && parent->txEntry.isRBF)
       && (child->txEntry.txHash != parent->txEntry.txHash) && (child->txEntry.id == parent->txEntry.id)) {
      std::set<BinaryData> childInputs, parentInputs;
      for (int i = 0; i < child->tx.getNumTxIn(); i++) {
         childInputs.insert(child->tx.getTxInCopy(i).serialize());
      }
      for (int i = 0; i < parent->tx.getNumTxIn(); i++) {
         parentInputs.insert(parent->tx.getTxInCopy(i).serialize());
      }
      if (childInputs == parentInputs) {
         return true;
      }
   }
   return false;
}

std::pair<size_t, size_t> TransactionsViewModel::updateTransactionsPage(const std::vector<bs::TXEntry> &page)
{
   auto newItems = std::make_shared<std::unordered_map<std::string, std::pair<TransactionPtr, TXNode *>>>();
   auto updatedItems = std::make_shared<std::vector<TransactionPtr>>();
   auto newTxKeys = std::make_shared<std::unordered_set<std::string>>();
   auto keysMutex = std::make_shared<QMutex>();

   {
      QMutexLocker locker(&updateMutex_);
      for (const auto &entry : page) {
         const auto item = itemFromTransaction(entry);
         if (!item->wallet) {
            continue;
         }
         if (txKeyExists(item->id())) {
            updatedItems->push_back(item);
            continue;
         }
         currentItems_[item->id()] = item;
         if (!oldestItem_.isSet() || (oldestItem_.txEntry.txTime >= item->txEntry.txTime)) {
            oldestItem_ = *item;
         }
         newTxKeys->insert(item->id());
         newItems->insert({ item->id(), { item, new TXNode(item) } });
      }

      if (!updatedItems->empty()) {
         updateBlockHeight(*updatedItems);
      }
   }

   const auto &cbInited = [this, newItems, newTxKeys, keysMutex, updatedItems]
         (const TransactionsViewItem *itemPtr) {
      if (!itemPtr || !itemPtr->initialized) {
         logger_->error("item is not inited");
         return;
      }
      if (newTxKeys->empty()) {
         logger_->warn("TX keys already empty");
         return;
      }
      QMutexLocker locker(keysMutex.get());
      newTxKeys->erase(itemPtr->id());
      if (newTxKeys->empty()) {
         std::unordered_set<std::string> deletedItems;
         if (rootNode_->hasChildren()) {
            std::vector<int> delRows;
            const auto &cbEachExisting = [this, newItems, &delRows](TXNode *txNode) {
               for (auto &newItem : *newItems) {
                  if (newItem.second.second->find(txNode->item()->id())
                     || txNode->find(newItem.first)) {   // avoid looped graphs
                     continue;
                  }
                  if (isChildOf(txNode->item(), newItem.second.first)) {
                     delRows.push_back(txNode->row());
                     auto &&children = txNode->children();
                     newItem.second.second->add(txNode);
                     for (auto &child : children) {
                        newItem.second.second->add(child);
                     }
                     txNode->clear(false);
                  }
                  else if (isChildOf(newItem.second.first, txNode->item())) {
                     // do nothing, yet
                  }
               }
            };
            {
               QMutexLocker locker(&updateMutex_);
               for (auto &child : rootNode_->children()) {
                  cbEachExisting(child);
               }
            }
            if (!delRows.empty()) {
               onDelRows(delRows);
            }
         }
         const auto newItemsCopy = *newItems;
         for (auto &parentItem : *newItems) {
            if (deletedItems.find(parentItem.first) != deletedItems.end()) {  // don't treat child-parent transitively
               continue;
            }
            for (auto &childItem : newItemsCopy) {
               if (parentItem.first == childItem.first) { // don't compare with self
                  continue;
               }
               if (deletedItems.find(childItem.first) != deletedItems.end()) {
                  continue;
               }
               if (isChildOf(childItem.second.first, parentItem.second.first)) {
                  parentItem.second.second->add(childItem.second.second);
                  deletedItems.insert(childItem.second.first->id());
               }
            }
         }
         for (const auto &delId : deletedItems) {
            newItems->erase(delId);
         }
         if (!newItems->empty()) {
            onNewItems(*newItems);
            if (signalOnEndLoading_ && updatedItems->empty()) {
               signalOnEndLoading_ = false;
               emit dataLoaded(newItems->size());
            }
         }
      }
   };

   const auto newItemsCopy = *newItems;
   if (!newItemsCopy.empty()) {
      for (auto item : newItemsCopy) {
         updateTransactionDetails(item.second.first, cbInited);
      }
   }
   else {
      emit dataLoaded(0);
   }

   return { newItemsCopy.size(), updatedItems->size() };
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
            onItemConfirmed(item);
         }
      }
   }
   emit dataChanged(index(0, static_cast<int>(Columns::Amount))
   , index(rootNode_->nbChildren() - 1, static_cast<int>(Columns::Status)));
}

void TransactionsViewModel::onItemConfirmed(const TransactionPtr item)
{
   if (item->txEntry.isRBF && (item->confirmations == 1)) {
      const auto node = rootNode_->find(item->id());
      if (node && node->hasChildren()) {
         beginRemoveRows(index(node->row(), 0), 0, node->nbChildren() - 1);
         node->clear();
         endRemoveRows();
      }
   }
}

void TransactionsViewModel::loadLedgerEntries()
{
   if (!initialLoadCompleted_ || !ledgerDelegate_) {
      return;
   }
   initialLoadCompleted_ = false;

   const auto &cbPageCount = [this](ReturnMessage<uint64_t> pageCnt)->void {
      try {
         auto inPageCnt = pageCnt.get();
         emit initProgress(0, inPageCnt * 2);
         for (uint64_t pageId = 0; pageId < inPageCnt; ++pageId) {
            if (stopped_) {
               logger_->debug("[TransactionsViewModel::loadLedgerEntries] stopped");
               break;
            }
            const auto &cbLedger = [this, pageId, inPageCnt]
               (ReturnMessage<std::vector<ClientClasses::LedgerEntry>> entries)->void {
               try {
                 auto le = entries.get();
                 rawData_[pageId] = bs::TXEntry::fromLedgerEntries(le);
                 emit updateProgress((int)pageId);
               }
               catch (std::exception& e) {
                  logger_->error("[TransactionsViewModel::loadLedgerEntries] " \
                     "Return data error (getPageCount) - {}", e.what());
               }

               if (rawData_.size() >= inPageCnt) {
                  QMetaObject::invokeMethod(this, [this] {
                     ledgerToTxData();
                  });
               }
            };
            ledgerDelegate_->getHistoryPage(pageId, cbLedger);
         }
      }
      catch (const std::exception &e) {
         logger_->error("[TransactionsViewModel::loadLedgerEntries] Return " \
            "data error (getPageCount) - {}", e.what());
      }
   };

   ledgerDelegate_->getPageCount(cbPageCount);
}

void TransactionsViewModel::ledgerToTxData()
{
   int pageCnt = 0;

   signalOnEndLoading_ = true;
   for (const auto &le : rawData_) {
      updateTransactionsPage(le.second);
      emit updateProgress(rawData_.size() + pageCnt++);
   }
   rawData_.clear();
   initialLoadCompleted_ = true;
}

void TransactionsViewModel::onNewItems(const std::unordered_map<std::string, std::pair<TransactionPtr, TXNode *>> &newItems)
{
   const int curLastIdx = rootNode_->nbChildren();
   beginInsertRows(QModelIndex(), curLastIdx, curLastIdx + newItems.size() - 1);
   {
      QMutexLocker locker(&updateMutex_);
      for (const auto &newItem : newItems) {
         rootNode_->add(newItem.second.second);
      }
   }
   endInsertRows();
}

void TransactionsViewModel::onDelRows(std::vector<int> rows)
{        // optimize for contiguous ranges, if needed
   std::sort(rows.begin(), rows.end());
   int rowCnt = rowCount();
   for (int i = 0; i < rows.size(); ++i) {
      const int row = rows[i] - i;  // special hack for correcting row index after previous row deletion
      if ((row < 0) || row >= rowCnt) {
         continue;
      }

      beginRemoveRows(QModelIndex(), row, row);
      rootNode_->del(row);
      endRemoveRows();
      rowCnt--;
   }
}

TransactionsViewItem TransactionsViewModel::getItem(const QModelIndex &index) const
{
   const auto &node = getNode(index);
   if (!node) {
      return {};
   }
   return *(node->item());
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


void TransactionsViewItem::initialize(ArmoryConnection *armory
   , const std::shared_ptr<bs::sync::WalletsManager> &walletsMgr
   , std::function<void(const TransactionsViewItem *)> userCB)
{
   const auto cbCheckIfInitializationCompleted = [this, userCB] {
      if (initialized) {
         return;
      }
      if (!dirStr.isEmpty() && !mainAddress.isEmpty() && !amountStr.isEmpty()) {
         initialized = true;
         userCB(this);
      }
   };

   const auto cbMainAddr = [this, cbCheckIfInitializationCompleted](QString mainAddr, int addrCount) {
      mainAddress = mainAddr;
      addressCount = addrCount;
      cbCheckIfInitializationCompleted();
   };

   const auto cbInit = [this, walletsMgr, cbMainAddr, cbCheckIfInitializationCompleted, userCB] {
      if (amountStr.isEmpty() && txHashesReceived) {
         calcAmount(walletsMgr);
      }
      if (mainAddress.isEmpty()) {
         if (!walletsMgr->getTransactionMainAddress(tx, walletID.toStdString(), (amount > 0), cbMainAddr)) {
            userCB(nullptr);
         }
      }
      else {
         cbCheckIfInitializationCompleted();
      }
   };

   const auto cbTXs = [this, cbInit, userCB](const std::vector<Tx> &txs) {
      for (const auto &tx : txs) {
         const auto &txHash = tx.getThisHash();
         txIns[txHash] = tx;
      }
      txHashesReceived = true;
      cbInit();
   };
   const auto &cbDir = [this, cbInit](bs::sync::Transaction::Direction dir, std::vector<bs::Address> inAddrs) {
      direction = dir;
      dirStr = QObject::tr(bs::sync::Transaction::toStringDir(dir));
      if (dir == bs::sync::Transaction::Direction::Received) {
         if (inAddrs.size() == 1) {    // likely a settlement address
            switch (inAddrs[0].getType()) {
            case AddressEntryType_P2WSH:
            case AddressEntryType_P2SH:
            case AddressEntryType_Multisig:
               parentId = inAddrs[0];
               break;
            default: break;
            }
         }
      }
      else if (dir == bs::sync::Transaction::Direction::Sent) {
         for (int i = 0; i < tx.getNumTxOut(); ++i) {
            TxOut out = tx.getTxOutCopy((int)i);
            bs::Address addr(out.getScrAddressStr());
            switch (addr.getType()) {
            case AddressEntryType_P2WSH:     // likely a settlement address
            case AddressEntryType_P2SH:
            case AddressEntryType_Multisig:
               parentId = addr;
               break;
            default: break;
            }
            if (!parentId.isNull()) {
               break;
            }
         }
      }
      else if (dir == bs::sync::Transaction::Direction::PayIn) {
         for (int i = 0; i < tx.getNumTxOut(); ++i) {
            TxOut out = tx.getTxOutCopy((int)i);
            bs::Address addr(out.getScrAddressStr());
            switch (addr.getType()) {
            case AddressEntryType_P2WSH:
            case AddressEntryType_P2SH:
            case AddressEntryType_Multisig:
               groupId = addr;
               break;
            default: break;
            }
            if (!groupId.isNull()) {
               break;
            }
         }
      }
      else if (dir == bs::sync::Transaction::Direction::PayOut) {
         if (inAddrs.size() == 1) {
            groupId = inAddrs[0];
         }
      }
      cbInit();
   };

   const auto cbTX = [this, armory, walletsMgr, cbTXs, cbInit, cbDir, cbMainAddr, userCB](const Tx &newTx) {
      if (!newTx.isInitialized()) {
         userCB(nullptr);
         return;
      }
      if (comment.isEmpty()) {
         comment = wallet ? QString::fromStdString(wallet->getTransactionComment(txEntry.txHash))
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
            txHashesReceived = true;
         }
         else {
            if (!armory->getTXsByHash(txHashSet, cbTXs)) {
               userCB(nullptr);
            }
         }
      }
      else {
         txHashesReceived = true;
      }

      if (dirStr.isEmpty()) {
         if (!walletsMgr->getTransactionDirection(tx, walletID.toStdString(), cbDir)) {
            userCB(nullptr);
         }
      }
      else {
         if (txHashesReceived) {
            cbInit();
         }
      }
   };

   if (initialized) {
      userCB(this);
   } else {
      if (tx.isInitialized()) {
         cbTX(tx);
      } else {
         if (!armory->getTxByHash(txEntry.txHash, cbTX)) {
            userCB(nullptr);
         }
      }
   }
}

static bool isSpecialWallet(const std::shared_ptr<bs::sync::Wallet> &wallet)
{
   if (!wallet) {
      return false;
   }
   switch (wallet->type()) {
   case bs::core::wallet::Type::Settlement:
   case bs::core::wallet::Type::ColorCoin:
      return true;
   default: break;
   }
   return false;
}

void TransactionsViewItem::calcAmount(const std::shared_ptr<bs::sync::WalletsManager> &walletsManager)
{
   if (wallet && tx.isInitialized()) {
      bool hasSpecialAddr = false;
      int64_t outputVal = 0;
      for (size_t i = 0; i < tx.getNumTxOut(); ++i) {
         TxOut out = tx.getTxOutCopy(i);
         if (txEntry.isChainedZC && !hasSpecialAddr) {
            const auto addr = bs::Address::fromTxOut(out);
            hasSpecialAddr = isSpecialWallet(walletsManager->getWalletByAddress(addr.id()));
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
               hasSpecialAddr = isSpecialWallet(walletsManager->getWalletByAddress(addr.id()));
            }
         }
      }
      auto value = txEntry.value;
      const auto fee = (wallet->type() == bs::core::wallet::Type::ColorCoin) || (value > 0) ? 0 : (outputVal - inputVal);
      value -= fee;
      amount = wallet->getTxBalance(value);
      amountStr = wallet->displayTxValue(value);

      if (txEntry.isChainedZC && (wallet->type() == bs::core::wallet::Type::Bitcoin) && !hasSpecialAddr) {
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
      && (wallet != nullptr && wallet->type() != bs::core::wallet::Type::Settlement)
      && (direction == bs::sync::Transaction::Direction::Internal
         || direction == bs::sync::Transaction::Direction::Sent));
}

bool TransactionsViewItem::isCPFPeligible() const
{
   return ((confirmations == 0) && (wallet != nullptr && wallet->type() != bs::core::wallet::Type::Settlement)
      && (direction == bs::sync::Transaction::Direction::Internal
         || direction == bs::sync::Transaction::Direction::Received));
}
