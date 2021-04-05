/*

***********************************************************************************
* Copyright (C) 2018 - 2021, BlockSettle AB
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
#include <spdlog/spdlog.h>
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
   colorUnknown_ = Qt::gray;
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
         if (item_->txOutIndex >= 0 && !item_->txMultipleOutIndex) {
            return QString::fromStdString(fmt::format("{}/{}", item_->txEntry.txHash.toHexStr(true), item_->txOutIndex));
         } else {
            return QString::fromStdString(item_->txEntry.txHash.toHexStr(true));
         }
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
         switch (item_->isValid) {
            case bs::sync::TxValidity::Unknown:    return colorUnknown_;
            case bs::sync::TxValidity::Valid:      return QVariant();
            case bs::sync::TxValidity::Invalid:    return colorInvalid_;
         }
         return colorInvalid_;
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
   auto node = take(index);
   if (node) {
      delete node;
   }
}

TXNode* TXNode::take(int index)
{
   if (index >= children_.size()) {
      return nullptr;
   }
   auto node = children_.takeAt(index);
   for (int i = index; i < children_.size(); ++i) {
      children_[i]->row_--;
   }
   return node;
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

TXNode* TXNode::find(const BinaryData& txHash) const
{
   if (item_ && (item_->txEntry.txHash == txHash)) {
      return const_cast<TXNode*>(this);
   }
   for (const auto& child : children_) {
      const auto found = child->find(txHash);
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


TransactionsViewModel::TransactionsViewModel(const std::shared_ptr<spdlog::logger> &logger
   , QObject* parent)
   : QAbstractItemModel(parent), logger_(logger)
   , allWallets_(true), filterAddress_()
{
   stopped_ = std::make_shared<std::atomic_bool>(false);
   rootNode_.reset(new TXNode);
}

TransactionsViewModel::~TransactionsViewModel() noexcept
{
   *stopped_ = true;
}

int TransactionsViewModel::columnCount(const QModelIndex &) const
{
   return static_cast<int>(Columns::last) + 1;
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
      case Columns::Status:
      case Columns::Amount:
         return Qt::AlignRight;
      case Columns::Flag:
         return Qt::AlignCenter;
      default:
         break;
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
//      loadAllWallets();
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

std::shared_ptr<TransactionsViewItem> TransactionsViewModel::createTxItem(const bs::TXEntry &entry)
{
   auto item = std::make_shared<TransactionsViewItem>();
   item->txEntry = entry;
   item->displayDateTime = UiUtils::displayDateTime(entry.txTime);
//   item->filterAddress = filterAddress_;
   if (!entry.walletIds.empty()) {
      item->walletID = QString::fromStdString(*entry.walletIds.cbegin());
   }
   item->confirmations = entry.nbConf;
   if (!item->wallets.empty()) {
      item->walletName = QString::fromStdString(item->wallets[0]->name());
   }
   item->amountStr = UiUtils::displayAmount(entry.value);
   return item;
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

void TransactionsViewModel::onRefreshTxValidity()
{
   for (int i = 0; i < rootNode_->children().size(); ++i) {
      const auto item = rootNode_->children()[i]->item();
      // This fixes race with CC tracker (when it updates after adding new TX).
      // So there is no need to check already valid TXs.
      if (!item || item->isValid == bs::sync::TxValidity::Valid) {
         continue;
      }
      const auto validWallet = item->wallets.empty() ? nullptr : item->wallets[0];
      auto newState = validWallet ? validWallet->isTxValid(item->txEntry.txHash) : bs::sync::TxValidity::Invalid;
      if (item->isValid != newState) {
         item->isValid = newState;
         // Update balance in case lotSize_ is received after CC gen file loaded
//         item->calcAmount(walletsManager_);
         emit dataChanged(index(i, static_cast<int>(Columns::first))
         , index(i, static_cast<int>(Columns::last)));
      }
   }
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

void TransactionsViewModel::onNewBlock(unsigned int curBlock)
{
   if (curBlock_ == curBlock) {
      return;
   }
   const unsigned int diff = curBlock - curBlock_;
   curBlock_ = curBlock;

   for (const auto &node : rootNode_->children()) {
      if (node->item()->confirmations == 0) {
         continue;
      }
      node->item()->confirmations += diff;
      node->item()->txEntry.nbConf += diff;
   }
   emit dataChanged(index(0, static_cast<int>(Columns::Status))
      , index(rootNode_->nbChildren() - 1, static_cast<int>(Columns::Status)));
}

void TransactionsViewModel::onLedgerEntries(const std::string &, uint32_t
   , uint32_t, uint32_t curBlock, const std::vector<bs::TXEntry> &entries)
{
   if (!curBlock_) {
      curBlock_ = curBlock;
   }
   else if (curBlock_ != curBlock) {
      curBlock_ = curBlock;
      onNewBlock(curBlock);
   }
   if (entries.empty()) {
      return;
   }

   std::vector<TXNode *> newNodes;
   for (const auto &entry : entries) {
      if (entry.txHash.empty()) {   // invalid entry
         continue;
      }
      const auto &item = createTxItem(entry);
      const auto &itItem = itemIndex_.find({entry.txHash, item->walletID.toStdString()});
      if (itItem == itemIndex_.end()) {
         newNodes.push_back(new TXNode(item));
      }
      else {
         const int row = itItem->second;
         rootNode_->children()[row]->setItem(item);
         emit dataChanged(index(row, static_cast<int>(Columns::first))
            , index(row, static_cast<int>(Columns::last)));
      }
   }

   if (!newNodes.empty()) {
      beginInsertRows(QModelIndex(), rootNode_->nbChildren()
         , rootNode_->nbChildren() + newNodes.size() - 1);
      for (const auto &node : newNodes) {
         itemIndex_[{node->item()->txEntry.txHash, node->item()->walletID.toStdString()}] = rootNode_->nbChildren();
         rootNode_->add(node);
      }
      endInsertRows();
   }

   std::vector<bs::sync::TXWallet> txWallet;
   txWallet.reserve(entries.size());
   for (const auto &entry : entries) {
      const auto &walletId = entry.walletIds.empty() ? std::string{} : *(entry.walletIds.cbegin());
      txWallet.push_back({ entry.txHash,  walletId, entry.value });
   }
   emit needTXDetails(txWallet, true, {});
}

void TransactionsViewModel::onZCsInvalidated(const std::vector<BinaryData>& txHashes)
{
   for (const auto& txHash : txHashes) {
      const auto node = rootNode_->find(txHash);
      if (node) {
         const int row = node->row();
         beginRemoveRows(QModelIndex(), row, row);
         auto removedNode = rootNode_->take(row);
         itemIndex_.erase({ node->item()->txEntry.txHash, node->item()->walletID.toStdString() });
         endRemoveRows();
         if (removedNode) {
            removedNode->item()->curBlock = curBlock_;
            invalidatedNodes_[txHash] = removedNode;
         }
      }
   }
   std::vector<bs::sync::TXWallet> txWallet;
   txWallet.reserve(invalidatedNodes_.size());
   for (const auto& invNode : invalidatedNodes_) {
      const auto& entry = invNode.second->item()->txEntry;
      const auto& walletId = entry.walletIds.empty() ? std::string{} : *(entry.walletIds.cbegin());
      txWallet.push_back({ entry.txHash,  walletId, entry.value });
   }
   emit needTXDetails(txWallet, false, {});
}

void TransactionsViewModel::onTXDetails(const std::vector<bs::sync::TXWalletDetails> &txDet)
{
   std::vector<TXNode*> newNodes;
   for (const auto &tx : txDet) {
      TransactionPtr item;
      int row = -1;
      const auto &itIndex = itemIndex_.find({tx.txHash, tx.walletId});
      if (itIndex == itemIndex_.end()) {
         const auto& itInv = invalidatedNodes_.find(tx.txHash);
         if (itInv == invalidatedNodes_.end()) {
            continue;
         }
         newNodes.push_back(itInv->second);
         item = itInv->second->item();
         invalidatedNodes_.erase(itInv);
      }
      else {
         row = itIndex->second;
         if (row >= rootNode_->nbChildren()) {
            logger_->warn("[{}] invalid row: {} of {}", __func__, row, rootNode_->nbChildren());
            continue;
         }
         item = rootNode_->children()[row]->item();
      }
      item->walletName = QString::fromStdString(tx.walletName);
      item->direction = tx.direction;
      item->dirStr = QObject::tr(bs::sync::Transaction::toStringDir(tx.direction));
      item->isValid = tx.isValid ? bs::sync::TxValidity::Valid : bs::sync::TxValidity::Invalid;
      if (!tx.comment.empty()) {
         item->comment = QString::fromStdString(tx.comment);
      }
      item->amountStr = QString::fromStdString(tx.amount);

      item->addressCount = tx.outAddresses.size();
      switch (tx.outAddresses.size()) {
      case 0:
         item->mainAddress = tr("no addresses");
         break;
      case 1:
         item->mainAddress = QString::fromStdString(tx.outAddresses[0].display());
         break;
      default:
         item->mainAddress = tr("%1 output addresses").arg(tx.outAddresses.size());
         break;
      }
      item->tx = tx.tx;
      item->inputAddresses = tx.inputAddresses;
      item->outputAddresses = tx.outputAddresses;
      item->changeAddress = tx.changeAddress;
      if (row >= 0) {
         emit dataChanged(index(row, static_cast<int>(Columns::Wallet))
            , index(row, static_cast<int>(Columns::Comment)));
      } else {
         item->confirmations = curBlock_ - item->curBlock + 1;
         //FIXME: this code doesn't provide proper conf #, even with caching turned off:
         // curBlock_ + 1 - tx.tx.getTxHeight();
         item->txEntry.nbConf = item->confirmations;
      }
   }
   if (!newNodes.empty()) {
      const int startRow = rootNode_->nbChildren();
      const int endRow = rootNode_->nbChildren() + newNodes.size() - 1;
      beginInsertRows(QModelIndex(), startRow, endRow);
      for (const auto& node : newNodes) {
         itemIndex_[{node->item()->txEntry.txHash, node->item()->walletID.toStdString()}] = rootNode_->nbChildren();
         rootNode_->add(node);
      }
      endInsertRows();
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
         TxOut out = tx.getTxOutCopy(i);
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
               if (txOutIndex >= 0) {
                  txMultipleOutIndex = true;
               }
               txOutIndex = out.getIndex();
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
         if (prevTx && prevTx->isInitialized()) {
            TxOut prevOut = prevTx->getTxOutCopy(op.getTxOutIndex());
            const auto addr = bs::Address::fromTxOut(prevTx->getTxOutCopy(op.getTxOutIndex()));
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
               if (txOutIndex >= 0) {
                  txMultipleOutIndex = true;
               }
               txOutIndex = in.getIndex();
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

bool TransactionsViewItem::isPayin() const
{
   bool hasSettlOut = false;
   for (int i = 0; i < tx.getNumTxOut(); ++i) {
      const auto &txOut = tx.getTxOutCopy(i);
      const auto &addr = bs::Address::fromTxOut(txOut);
      hasSettlOut |= addr.getType() == AddressEntryType_P2WSH;
   }
   return hasSettlOut && (direction == bs::sync::Transaction::Direction::Sent);
}
