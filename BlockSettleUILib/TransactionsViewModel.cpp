/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "TransactionsViewModel.h"

#include "ArmoryConnection.h"
#include "CheckRecipSigner.h"
#include "UiUtils.h"
#include "Wallets/SyncWalletsManager.h"

#include <QApplication>
#include <QDateTime>
#include <QMutexLocker>
#include <QFutureWatcher>


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

   colorGray_ = Qt::darkGray;
   colorRed_ = Qt::red;
   colorYellow_ = Qt::darkYellow;
   colorGreen_ = Qt::darkGreen;
   colorInvalid_ = Qt::red;
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
      return qVariantFromValue(static_cast<void*>(item_->wallets.empty() ? nullptr : item_->wallets[0].get()));
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

void TXNode::forEach(const std::function<void(const TransactionPtr &)> &cb)
{
   if (item_) {
      cb(item_);
   }
   for (auto child : children_) {
      child->forEach(cb);
   }
}

TXNode *TXNode::find(const bs::TXEntry &entry) const
{
   if (item_ && (item_->txEntry.txHash == entry.txHash)) {
      if (item_->txEntry.walletIds == entry.walletIds) {
         return const_cast<TXNode*>(this);
      }
      if (entry.walletIds.size() < item_->txEntry.walletIds.size()) {
         for (const auto &walletId : entry.walletIds) {
            if (item_->txEntry.walletIds.find(walletId) != item_->txEntry.walletIds.end()) {
               return const_cast<TXNode*>(this);
            }
         }
      }
      else {
         for (const auto &walletId : item_->txEntry.walletIds) {
            if (entry.walletIds.find(walletId) != entry.walletIds.end()) {
               return const_cast<TXNode*>(this);
            }
         }
      }
   }
   for (const auto &child : children_) {
      const auto found = child->find(entry);
      if (found != nullptr) {
         return found;
      }
   }
   return nullptr;
}

std::vector<TXNode *> TXNode::nodesByTxHash(const BinaryData &txHash) const
{
   std::vector<TXNode *> result;
   if (item_ && (item_->txEntry.txHash == txHash)) {
      result.push_back(const_cast<TXNode*>(this));
   }
   for (const auto &child : children_) {
      const auto childNodes = child->nodesByTxHash(txHash);
      result.insert(result.end(), childNodes.cbegin(), childNodes.cend());
   }
   return result;
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
                         , const std::shared_ptr<bs::sync::Wallet> &defWlt
                         , const bs::Address &filterAddress
                         , QObject* parent)
   : QAbstractItemModel(parent)
   , logger_(logger)
   , ledgerDelegate_(ledgerDelegate)
   , walletsManager_(walletsManager)
   , defaultWallet_(defWlt)
   , allWallets_(false)
   , filterAddress_(filterAddress)
{
   init();
   ArmoryCallbackTarget::init(armory.get());
   loadLedgerEntries();
}

TransactionsViewModel::TransactionsViewModel(const std::shared_ptr<ArmoryConnection> &armory
                         , const std::shared_ptr<bs::sync::WalletsManager> &walletsManager
                                 , const std::shared_ptr<spdlog::logger> &logger
                                             , QObject* parent)
   : QAbstractItemModel(parent)
   , logger_(logger)
   , walletsManager_(walletsManager)
   , allWallets_(true)
{
   ArmoryCallbackTarget::init(armory.get());
   init();
}

void TransactionsViewModel::init()
{
   stopped_ = std::make_shared<std::atomic_bool>(false);
   qRegisterMetaType<TransactionsViewItem>();
   qRegisterMetaType<TransactionItems>();

   rootNode_.reset(new TXNode);

   connect(walletsManager_.get(), &bs::sync::WalletsManager::walletChanged, this, &TransactionsViewModel::refresh, Qt::QueuedConnection);
   connect(walletsManager_.get(), &bs::sync::WalletsManager::walletDeleted, this, &TransactionsViewModel::onWalletDeleted, Qt::QueuedConnection);
   connect(walletsManager_.get(), &bs::sync::WalletsManager::walletImportFinished, this, &TransactionsViewModel::refresh, Qt::QueuedConnection);
   connect(walletsManager_.get(), &bs::sync::WalletsManager::walletsReady, this, &TransactionsViewModel::updatePage, Qt::QueuedConnection);
}

TransactionsViewModel::~TransactionsViewModel() noexcept
{
   cleanup();
   *stopped_ = true;
}

void TransactionsViewModel::onNewBlock(unsigned int, unsigned int)
{
   QMetaObject::invokeMethod(this, [this] {
      if (allWallets_) {
         loadAllWallets(true);
      }
   });
}

void TransactionsViewModel::loadAllWallets(bool onNewBlock)
{
   const auto &cbWalletsLD = [this, onNewBlock](const std::shared_ptr<AsyncClient::LedgerDelegate> &delegate) {
      if (!initialLoadCompleted_) {
         if (onNewBlock && logger_) {
            logger_->debug("[TransactionsViewModel::loadAllWallets] previous loading is not complete, yet");
         }
         return;
      }
      ledgerDelegate_ = delegate;
      if (onNewBlock && logger_) {
         logger_->debug("[TransactionsViewModel::loadAllWallets] ledger delegate is updated");
      }
      loadLedgerEntries(onNewBlock);
   };
   if (initialLoadCompleted_) {
      if (ledgerDelegate_) {
         loadLedgerEntries(onNewBlock);
      }
      else {
         armory_->getWalletsLedgerDelegate(cbWalletsLD);
      }
   }
}

int TransactionsViewModel::columnCount(const QModelIndex &) const
{
   return static_cast<int>(Columns::last);
}

TXNode *TransactionsViewModel::getNode(const QModelIndex &index) const
{
   if (!index.isValid()) {
      return rootNode_.get();
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
   if ((parentNode == nullptr) || (parentNode == rootNode_.get())) {
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
   *stopped_ = true;
   beginResetModel();
   {
      QMutexLocker locker(&updateMutex_);
      rootNode_->clear();
      oldestItem_ = {};
   }
   endResetModel();
   *stopped_ = false;
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
   for (const auto &walletId : entry.walletIds) {
      const auto wallet = walletsManager_->getWalletById(walletId);
      if (wallet) {
         item->wallets.push_back(wallet);
      }
   }
   item->filterAddress = filterAddress_;
   if (item->wallets.empty() && defaultWallet_) {
      item->wallets.push_back(defaultWallet_);
   }
   if (!item->wallets.empty()) {
      item->walletID = QString::fromStdString(item->wallets[0]->walletId());
   }
   else {
      item->walletID = QString::fromStdString(*entry.walletIds.cbegin());
   }

   item->confirmations = armory_->getConfirmationsNumber(entry.blockNum);
   if (!item->wallets.empty()) {
      item->walletName = QString::fromStdString(item->wallets[0]->name());
   }
   const auto validWallet = item->wallets.empty() ? nullptr : item->wallets[0];
   item->isValid = validWallet ? validWallet->isTxValid(entry.txHash) : false;
   return item;
}

void TransactionsViewModel::onZCReceived(const std::vector<bs::TXEntry> &entries)
{
   QMetaObject::invokeMethod(this, [this, entries] { updateTransactionsPage(entries); });
}

void TransactionsViewModel::onZCInvalidated(const std::set<BinaryData> &ids)
{
   std::vector<int> delRows;
#ifdef TX_MODEL_NESTED_NODES
   std::vector<bs::TXEntry> children;
#endif
   {
      QMutexLocker locker(&updateMutex_);
      for (const auto &txHash : ids) {
         const auto invNodes = rootNode_->nodesByTxHash(txHash);
         for (const auto &node : invNodes) {
            delRows.push_back(node->row());
         }
#ifdef TX_MODEL_NESTED_NODES // nested nodes are not supported for now
         const auto node = rootNode_->find(entry);
         if (node && (node->parent() == rootNode_.get()) && !node->item()->confirmations) {
            delRows.push_back(node->row());
            if (node->hasChildren()) { // handle race condition when node being deleted has confirmed children
               for (const auto &child : node->children()) {
                  if (child->item()->confirmations) {
                     children.push_back(child->item()->txEntry);
                  }
               }
            }
         }
#endif   //TX_MODEL_NESTED_NODES
      }
   }
   if (!delRows.empty()) {
      onDelRows(delRows);
   }

#ifdef TX_MODEL_NESTED_NODES
   if (!children.empty()) {
      logger_->debug("[{}] {} children to update", __func__, children.size());
      updateTransactionsPage(children);
   }
#endif
}

#ifdef TX_MODEL_NESTED_NODES
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
      && (child->txEntry.txHash != parent->txEntry.txHash)
      && (child->txEntry.walletIds == parent->txEntry.walletIds)) {
      std::set<BinaryData> childInputs, parentInputs;
      for (int i = 0; i < int(child->tx.getNumTxIn()); i++) {
         childInputs.insert(child->tx.getTxInCopy(i).serialize());
      }
      for (int i = 0; i < int(parent->tx.getNumTxIn()); i++) {
         parentInputs.insert(parent->tx.getTxInCopy(i).serialize());
      }
      if (childInputs == parentInputs) {
         return true;
      }
   }
   return false;
}
#endif   //TX_MODEL_NESTED_NODES

std::pair<size_t, size_t> TransactionsViewModel::updateTransactionsPage(const std::vector<bs::TXEntry> &page)
{
   struct ItemKey {
      BinaryData  txHash;
      std::set<std::string>   walletIds;
      bool operator<(const ItemKey &ik) const {
         if (txHash != ik.txHash) {
            return (txHash < ik.txHash);
         }
         return (walletIds < ik.walletIds);
      }
   };
   auto newItems = std::make_shared<std::vector<TXNode *>>();
   auto updatedItems = std::make_shared<std::vector<TransactionPtr>>();
   auto newTxKeys = std::make_shared<std::set<ItemKey>>();

   const auto mergedPage = allWallets_ ? walletsManager_->mergeEntries(page) : page;

   const auto lbdAddNew = [this, newItems, newTxKeys](const TransactionPtr &item)
   {
      if (!oldestItem_ || (oldestItem_->txEntry.txTime >= item->txEntry.txTime)) {
         oldestItem_ = item;
      }
      newTxKeys->insert({ item->txEntry.txHash, item->txEntry.walletIds });
      newItems->push_back(new TXNode(item));
   };

   const auto mergeItem = [this, updatedItems](const TransactionPtr &item) -> bool
   {
      for (const auto &node : rootNode_->children()) {
         if (!node) {
            continue;
         }
         if (walletsManager_->mergeableEntries(node->item()->txEntry, item->txEntry)) {
            item->txEntry.merge(node->item()->txEntry);
            updatedItems->push_back(item);
            return true;
         }
      }
      return false;
   };

   for (const auto &entry : mergedPage) {
      const auto item = itemFromTransaction(entry);
      if (item->wallets.empty()) {
         continue;
      }

      TXNode *node = nullptr;
      {
         QMutexLocker locker(&updateMutex_);
         const auto node = rootNode_->find(item->txEntry);
      }
      if (node) {
         updatedItems->push_back(item);
      }
      else {
         if (allWallets_) {
            if (!mergeItem(item)) {
               lbdAddNew(item);
            }
         }
         else {
            lbdAddNew(item);
         }
      }
   }

   const auto &cbInited = [this, newItems, updatedItems, newTxKeys]
      (const TransactionPtr &itemPtr)
   {
      if (!itemPtr || !itemPtr->initialized) {
         logger_->error("item is not inited");
         return;
      }
      if (newTxKeys->empty()) {
         logger_->warn("TX keys already empty");
         return;
      }
      newTxKeys->erase({ itemPtr->txEntry.txHash, itemPtr->txEntry.walletIds });
      if (newTxKeys->empty()) {
#ifdef TX_MODEL_NESTED_NODES
         std::unordered_set<std::string> deletedItems;
         if (rootNode_->hasChildren()) {
            std::vector<int> delRows;
            const auto &cbEachExisting = [newItems, &delRows](TXNode *txNode) {
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
#endif   //TX_MODEL_NESTED_NODES
         if (!newItems->empty()) {
            onNewItems(*newItems);
            if (signalOnEndLoading_) {
               signalOnEndLoading_ = false;
               emit dataLoaded(int(newItems->size()));
            }
         }
         if (!updatedItems->empty()) {
            updateBlockHeight(*updatedItems);
         }
      }
   };

   const auto newItemsCopy = *newItems;
   if (!newItemsCopy.empty()) {
      for (const auto &node : newItemsCopy) {
         updateTransactionDetails(node->item(), cbInited);
      }
   }
   else {
      if (!updatedItems->empty()) {
         updateBlockHeight(*updatedItems);
      }
      emit dataLoaded(0);
   }

   return { newItemsCopy.size(), updatedItems->size() };
}

void TransactionsViewModel::updateBlockHeight(const std::vector<std::shared_ptr<TransactionsViewItem>> &updItems)
{
   if (!rootNode_->hasChildren()) {
      logger_->debug("[{}] root node doesn't have children", __func__);
      return;
   }

   for (const auto &updItem : updItems) {
      TXNode *node = nullptr;
      {
         QMutexLocker locker(&updateMutex_);
         node = rootNode_->find(updItem->txEntry);
      }
      if (!node) {
         continue;
      }
      const auto &item = node->item();
      if (!updItem->wallets.empty()) {
         item->isValid = updItem->wallets[0]->isTxValid(updItem->txEntry.txHash);
      }
      if (item->txEntry.value != updItem->txEntry.value) {
         item->wallets = updItem->wallets;
         item->walletID = updItem->walletID;
         item->txEntry = updItem->txEntry;
         item->amountStr.clear();
         item->calcAmount(walletsManager_);
      }
      const auto newBlockNum = updItem->txEntry.blockNum;
      if (newBlockNum != UINT32_MAX) {
         const auto confNum = armory_->getConfirmationsNumber(newBlockNum);
         item->confirmations = confNum;
         item->txEntry.blockNum = newBlockNum;
         onItemConfirmed(item);
      }
   }

   emit dataChanged(index(0, static_cast<int>(Columns::Amount))
   , index(rootNode_->nbChildren() - 1, static_cast<int>(Columns::Status)));
}

void TransactionsViewModel::onItemConfirmed(const TransactionPtr item)
{
   if (item->txEntry.isRBF && (item->confirmations == 1)) {
      const auto node = rootNode_->find(item->txEntry);
      if (node && node->hasChildren()) {
         beginRemoveRows(index(node->row(), 0), 0, node->nbChildren() - 1);
         node->clear();
         endRemoveRows();
      }
   }
}

void TransactionsViewModel::loadLedgerEntries(bool onNewBlock)
{
   if (!initialLoadCompleted_ || !ledgerDelegate_) {
      if (onNewBlock && logger_) {
         logger_->debug("[TransactionsViewModel::loadLedgerEntries] previous loading is not complete/started");
      }
      return;
   }
   initialLoadCompleted_ = false;

   QPointer<TransactionsViewModel> thisPtr = this;
   auto rawData = std::make_shared<std::map<int, std::vector<bs::TXEntry>>>();
   auto rawDataMutex = std::make_shared<std::mutex>();

   const auto &cbPageCount = [thisPtr, onNewBlock, stopped = stopped_, logger = logger_, rawData, rawDataMutex, ledgerDelegate = ledgerDelegate_]
      (ReturnMessage<uint64_t> pageCnt)
   {
      try {
         int inPageCnt = int(pageCnt.get());

         QMetaObject::invokeMethod(qApp, [thisPtr, inPageCnt] {
            if (thisPtr) {
               emit thisPtr->initProgress(0, int(inPageCnt * 2));
            }
         });

         for (int pageId = 0; pageId < inPageCnt; ++pageId) {
            if (*stopped) {
               logger->debug("[TransactionsViewModel::loadLedgerEntries] stopped");
               break;
            }

            const auto &cbLedger = [thisPtr, onNewBlock, pageId, inPageCnt, rawData, logger, rawDataMutex]
               (ReturnMessage<std::vector<ClientClasses::LedgerEntry>> entries)->void {
               try {
                  auto le = entries.get();

                  std::lock_guard<std::mutex> lock(*rawDataMutex);

                  (*rawData)[pageId] = bs::TXEntry::fromLedgerEntries(le);
                  if (onNewBlock && logger) {
                     logger->debug("[TransactionsViewModel::loadLedgerEntries] loaded {} entries for page {} (of {})"
                        , le.size(), pageId, inPageCnt);
                  }

                  if (int(rawData->size()) >= inPageCnt) {
                     QMetaObject::invokeMethod(qApp, [thisPtr, rawData, onNewBlock] {
                        if (thisPtr) {
                           thisPtr->ledgerToTxData(*rawData, onNewBlock);
                        }
                     });
                  }
               }
               catch (std::exception& e) {
                  logger->error("[TransactionsViewModel::loadLedgerEntries::cbLedger] " \
                     "return data error: {}", e.what());
               }

               QMetaObject::invokeMethod(qApp, [thisPtr, pageId] {
                  if (thisPtr) {
                     emit thisPtr->updateProgress(pageId);
                  }
               });
            };
            ledgerDelegate->getHistoryPage(uint32_t(pageId), cbLedger);
         }
      }
      catch (const std::exception &e) {
         logger->error("[TransactionsViewModel::loadLedgerEntries::cbPageCount] return " \
            "data error: {}", e.what());
      }
   };

   ledgerDelegate_->getPageCount(cbPageCount);
}

void TransactionsViewModel::ledgerToTxData(const std::map<int, std::vector<bs::TXEntry>> &rawData
   , bool onNewBlock)
{
   int pageCnt = 0;

   signalOnEndLoading_ = true;
   for (const auto &le : rawData) {
      const auto result = updateTransactionsPage(le.second);
      emit updateProgress(int(rawData.size()) + pageCnt++);
   }
   initialLoadCompleted_ = true;
}

void TransactionsViewModel::onNewItems(const std::vector<TXNode *> &newItems)
{
   const int curLastIdx = rootNode_->nbChildren();

   // That is less expensive just to compare first two list are the same - O(n)
   // then lookup O(n^2) straight away in next block
   if (rootNode_->children().size() == newItems.size()) {
      bool isEqual = std::equal(newItems.begin(), newItems.end(), rootNode_->children().begin(), [](TXNode const * const left, TXNode const * const right) {
         return left->item()->txEntry == right->item()->txEntry;
      });

      if (isEqual) {
         return;
      }
   }

   std::vector<TXNode *> actualChanges(newItems.size());
   int nextInserPosition = 0;
   for (const auto &newItem : newItems) {
      if (rootNode_->find(newItem->item()->txEntry)) {
         continue;
      }
      actualChanges[nextInserPosition++] = newItem;
   }
   actualChanges.resize(nextInserPosition);

   if (actualChanges.empty()) {
      return;
   }

   {
      QMutexLocker locker(&updateMutex_);
      beginInsertRows(QModelIndex(), curLastIdx, curLastIdx + actualChanges.size() - 1);
      for (const auto &newItem : actualChanges) {
         rootNode_->add(newItem);
      }
      endInsertRows();
   }
}

void TransactionsViewModel::onDelRows(std::vector<int> rows)
{        // optimize for contiguous ranges, if needed
   std::sort(rows.begin(), rows.end());
   int rowCnt = rowCount();
   QMutexLocker locker(&updateMutex_);
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

TransactionPtr TransactionsViewModel::getItem(const QModelIndex &index) const
{
   const auto &node = getNode(index);
   if (!node) {
      return {};
   }
   return node->item();
}

void TransactionsViewModel::updateTransactionDetails(const TransactionPtr &item
   , const std::function<void(const TransactionPtr &)> &cb)
{
   const auto &cbInited = [cb](const TransactionPtr &item) {
      if (cb) {
         cb(item);
      }
   };
   TransactionsViewItem::initialize(item, armory_, walletsManager_, cbInited);
}


void TransactionsViewItem::initialize(const TransactionPtr &item, ArmoryConnection *armory
   , const std::shared_ptr<bs::sync::WalletsManager> &walletsMgr
   , std::function<void(const TransactionPtr &)> userCB)
{
   const auto cbCheckIfInitializationCompleted = [item, userCB] {
      if (item->initialized) {
         return;
      }
      if (!item->dirStr.isEmpty() && !item->mainAddress.isEmpty() && !item->amountStr.isEmpty()) {
         item->initialized = true;
         userCB(item);
      }
   };

   const auto cbMainAddr = [item, cbCheckIfInitializationCompleted](QString mainAddr, int addrCount) {
      item->mainAddress = mainAddr;
      item->addressCount = addrCount;
      cbCheckIfInitializationCompleted();
   };

   const auto cbInit = [item, walletsMgr, cbMainAddr, cbCheckIfInitializationCompleted, userCB] {
      if (item->amountStr.isEmpty() && item->txHashesReceived) {
         item->calcAmount(walletsMgr);
      }
      if (item->mainAddress.isEmpty()) {
         if (!walletsMgr->getTransactionMainAddress(item->tx, item->walletID.toStdString(), (item->amount > 0), cbMainAddr)) {
            userCB(nullptr);
         }
      }
      else {
         cbCheckIfInitializationCompleted();
      }
   };

   const auto cbTXs = [item, cbInit, userCB]
      (const std::vector<Tx> &txs, std::exception_ptr exPtr)
   {
      if (exPtr != nullptr) {
         userCB(nullptr);
         return;
      }
      for (const auto &tx : txs) {
         const auto &txHash = tx.getThisHash();
         item->txIns[txHash] = tx;
      }
      item->txHashesReceived = true;
      cbInit();
   };
   const auto &cbDir = [item, cbInit](bs::sync::Transaction::Direction dir, std::vector<bs::Address> inAddrs) {
      item->direction = dir;
      item->dirStr = QObject::tr(bs::sync::Transaction::toStringDir(dir));
      if (dir == bs::sync::Transaction::Direction::Received) {
         if (inAddrs.size() == 1) {    // likely a settlement address
            switch (inAddrs[0].getType()) {
            case AddressEntryType_P2WSH:
            case AddressEntryType_P2SH:
            case AddressEntryType_Multisig:
               item->parentId = inAddrs[0];
               break;
            default: break;
            }
         }
      }
      else if (dir == bs::sync::Transaction::Direction::Sent) {
         for (int i = 0; i < item->tx.getNumTxOut(); ++i) {
            TxOut out = item->tx.getTxOutCopy((int)i);
            auto addr = bs::Address::fromHash(out.getScrAddressStr());
            switch (addr.getType()) {
            case AddressEntryType_P2WSH:     // likely a settlement address
            case AddressEntryType_P2SH:
            case AddressEntryType_Multisig:
               item->parentId = addr;
               break;
            default: break;
            }
            if (!item->parentId.isNull()) {
               break;
            }
         }
      }
      else if (dir == bs::sync::Transaction::Direction::PayIn) {
         for (int i = 0; i < item->tx.getNumTxOut(); ++i) {
            TxOut out = item->tx.getTxOutCopy((int)i);
            auto addr = bs::Address::fromHash(out.getScrAddressStr());
            switch (addr.getType()) {
            case AddressEntryType_P2WSH:
            case AddressEntryType_P2SH:
            case AddressEntryType_Multisig:
               item->groupId = addr;
               break;
            default: break;
            }
            if (!item->groupId.isNull()) {
               break;
            }
         }
      }
      else if (dir == bs::sync::Transaction::Direction::PayOut) {
         if (inAddrs.size() == 1) {
            item->groupId = inAddrs[0];
         }
      }
      cbInit();
   };

   const auto cbTX = [item, armory, walletsMgr, cbTXs, cbInit, cbDir, cbMainAddr, userCB](const Tx &newTx) {
      if (!newTx.isInitialized()) {
         userCB(nullptr);
         return;
      }
      if (item->comment.isEmpty()) {
         item->comment = item->wallets.empty() ? QString()
            : QString::fromStdString(item->wallets[0]->getTransactionComment(item->txEntry.txHash));
         const auto endLineIndex = item->comment.indexOf(QLatin1Char('\n'));
         if (endLineIndex != -1) {
            item->comment = item->comment.left(endLineIndex) + QLatin1String("...");
         }
      }

      if (!item->tx.isInitialized()) {
         item->tx = std::move(newTx);
         std::set<BinaryData> txHashSet;
         for (size_t i = 0; i < item->tx.getNumTxIn(); i++) {
            TxIn in = item->tx.getTxInCopy(i);
            OutPoint op = in.getOutPoint();
            if (item->txIns.find(op.getTxHash()) == item->txIns.end()) {
               txHashSet.insert(op.getTxHash());
            }
         }
         if (txHashSet.empty()) {
            item->txHashesReceived = true;
         }
         else {
            if (!armory->getTXsByHash(txHashSet, cbTXs)) {
               userCB(nullptr);
            }
         }
      }
      else {
         item->txHashesReceived = true;
      }

      if (item->dirStr.isEmpty()) {
         if (!walletsMgr->getTransactionDirection(item->tx, item->walletID.toStdString(), cbDir)) {
            userCB(nullptr);
         }
      }
      else {
         if (item->txHashesReceived) {
            cbInit();
         }
      }
   };

   if (item->initialized) {
      userCB(item);
   } else {
      if (item->tx.isInitialized()) {
         cbTX(item->tx);
      } else {
         if (!armory->getTxByHash(item->txEntry.txHash, cbTX)) {
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
   if (!wallets.empty() && tx.isInitialized()) {
      bool hasSpecialAddr = false;
      int64_t totalVal = 0;
      int64_t addressVal = 0;

      std::shared_ptr<bs::sync::Wallet> totalValWallet;
      for (size_t i = 0; i < tx.getNumTxOut(); ++i) {
         const TxOut out = tx.getTxOutCopy(i);
         try {
            const auto addr = bs::Address::fromTxOut(out);
            const auto addrWallet = walletsManager->getWalletByAddress(addr);
            if (txEntry.isChainedZC && !hasSpecialAddr && addrWallet) {
               hasSpecialAddr = isSpecialWallet(addrWallet);
            }
            for (const auto &wallet : wallets) {
               if (addrWallet == wallet) {
                  totalVal += out.getValue();
                  totalValWallet = wallet;
                  break;
               }
            }
            if (filterAddress.isValid() && addr == filterAddress) {
               addressVal += out.getValue();
            }
         } catch (const std::exception &) {
            addressVal += out.getValue();
         }  // ignore for OP_RETURN
      }

      std::shared_ptr<bs::sync::Wallet> addrValWallet;
      for (size_t i = 0; i < tx.getNumTxIn(); i++) {
         TxIn in = tx.getTxInCopy(i);
         OutPoint op = in.getOutPoint();
         const auto &prevTx = txIns[op.getTxHash()];
         if (prevTx.isInitialized()) {
            TxOut prevOut = prevTx.getTxOutCopy(op.getTxOutIndex());
            const auto addr = bs::Address::fromTxOut(prevTx.getTxOutCopy(op.getTxOutIndex()));
            const auto addrWallet = walletsManager->getWalletByAddress(addr);

            if (txEntry.isChainedZC && !hasSpecialAddr) {
               hasSpecialAddr = isSpecialWallet(walletsManager->getWalletByAddress(addr));
            }

            for (const auto &wallet : wallets) {
               if (addrWallet == wallet) {
                  totalVal -= prevOut.getValue();
                  addrValWallet = addrWallet;
                  if (!filterAddress.isValid()) {
                     addressVal -= prevOut.getValue();
                  }
                  break;
               }
            }

            if (filterAddress.isValid() && filterAddress == addr) {
               addressVal -= prevOut.getValue();
            }
         }
      }
      if (!filterAddress.isValid() && totalValWallet) {
         amount = totalValWallet->getTxBalance(totalVal);
         amountStr = totalValWallet->displayTxValue(totalVal);
      } else if (addrValWallet) {
         amount = addrValWallet->getTxBalance(addressVal);
         amountStr = addrValWallet->displayTxValue(addressVal);
      } else {
         amount = addressVal / BTCNumericTypes::BalanceDivider;
         amountStr = UiUtils::displayAmount(amount);
      }

      if (txEntry.isChainedZC && !wallets.empty()
         && (wallets[0]->type() == bs::core::wallet::Type::Bitcoin) && !hasSpecialAddr) {
         isCPFP = true;
      }
   }
   if (amount == 0) {
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

bool TransactionsViewItem::isRBFeligible() const
{
   return ((confirmations == 0) && txEntry.isRBF
      && (!wallets.empty() && wallets[0]->type() != bs::core::wallet::Type::Settlement)
      && (direction == bs::sync::Transaction::Direction::Internal
         || direction == bs::sync::Transaction::Direction::Sent));
}

bool TransactionsViewItem::isCPFPeligible() const
{
   return ((confirmations == 0) && (!wallets.empty() && wallets[0]->type() != bs::core::wallet::Type::Settlement)
      && (direction == bs::sync::Transaction::Direction::Internal
         || direction == bs::sync::Transaction::Direction::Received));
}
