/*

***********************************************************************************
* Copyright (C) 2018 - 2021, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __TRANSACTIONS_VIEW_MODEL_H__
#define __TRANSACTIONS_VIEW_MODEL_H__

#include <deque>
#include <unordered_set>
#include <QAbstractItemModel>
#include <QMutex>
#include <QColor>
#include <QFont>
#include <QMetaType>
#include <QTimer>
#include <atomic>
#include "AsyncClient.h"
#include "Wallets/SignerDefs.h"

namespace spdlog {
   class logger;
}
namespace bs {
   namespace sync {
      class Wallet;
      class WalletsManager;
   }
}
class SafeLedgerDelegate;

struct TransactionsViewItem;
using TransactionPtr = std::shared_ptr<TransactionsViewItem>;

struct TransactionsViewItem
{
   bs::TXEntry txEntry;
   Tx tx;
   std::vector<Tx>   prevTXs;
   bool initialized = false;
   QString mainAddress;
   int addressCount;
   std::vector<bs::Address> outAddresses;
   bs::sync::Transaction::Direction direction = bs::sync::Transaction::Unknown;
   std::vector<std::shared_ptr<bs::sync::Wallet>> wallets;
   std::vector<bs::sync::AddressDetails>  inputAddresses;
   std::vector<bs::sync::AddressDetails>  outputAddresses;
   bs::sync::AddressDetails   changeAddress;
   QString dirStr;
   QString walletName;
   QString walletID;
   QString comment;
   QString displayDateTime;
   QString amountStr;
   BTCNumericTypes::balance_type amount = 0;
   int32_t txOutIndex{-1};
   bool txMultipleOutIndex{false};
   bs::sync::TxValidity isValid = bs::sync::TxValidity::Unknown;
   bool     isCPFP = false;
   int confirmations = 0;

   BinaryData  parentId;   // universal grouping support
   BinaryData  groupId;

   bool isSet() const { return (!txEntry.txHash.empty() && !walletID.isEmpty()); }
   void calcAmount(const std::shared_ptr<bs::sync::WalletsManager> &);
   bool containsInputsFrom(const Tx &tx) const;

   bool isRBFeligible() const;
   bool isCPFPeligible() const;
   bool isPayin() const;

   bs::Address filterAddress;
   uint32_t    curBlock{ 0 };

private:
   bool     txHashesReceived{ false };
   AsyncClient::TxBatchResult txIns;
};
typedef std::vector<TransactionsViewItem>    TransactionItems;

class TXNode
{
public:
   TXNode();
   TXNode(const std::shared_ptr<TransactionsViewItem> &, TXNode *parent = nullptr);
   ~TXNode() { clear(); }

   std::shared_ptr<TransactionsViewItem> item() const { return item_; }
   void setItem(const std::shared_ptr<TransactionsViewItem> &item) { item_ = item; }
   size_t nbChildren() const { return children_.size(); }
   bool hasChildren() const { return !children_.empty(); }
   TXNode *child(int index) const;
   const QList<TXNode *> &children() const { return children_; }
   TXNode *parent() const { return parent_; }
   TXNode *find(const bs::TXEntry &) const;
   TXNode* find(const BinaryData &txHash) const;
   std::vector<TXNode *> nodesByTxHash(const BinaryData &) const;

   void clear(bool del = true);
   void setData(const TransactionsViewItem &data) { *item_ = data; }
   void add(TXNode *child);
   void del(int index);
   TXNode *take(int index);
   int row() const { return row_; }
   unsigned int level() const;
   QVariant data(int, int) const;

   void forEach(const std::function<void(const TransactionPtr &)> &);

private:
   void init();

private:
   std::shared_ptr<TransactionsViewItem>  item_;
   QList<TXNode *>   children_;
   int      row_ = 0;
   TXNode*  parent_ = nullptr;
   QFont    fontBold_;
   QColor   colorGray_, colorRed_, colorYellow_, colorGreen_, colorInvalid_, colorUnknown_;
};

Q_DECLARE_METATYPE(TransactionsViewItem)
Q_DECLARE_METATYPE(TransactionItems)


class TransactionsViewModel : public QAbstractItemModel
{
Q_OBJECT
public:
   TransactionsViewModel(const std::shared_ptr<spdlog::logger> &, QObject* parent = nullptr);
   ~TransactionsViewModel() noexcept override;

   TransactionsViewModel(const TransactionsViewModel&) = delete;
   TransactionsViewModel& operator = (const TransactionsViewModel&) = delete;
   TransactionsViewModel(TransactionsViewModel&&) = delete;
   TransactionsViewModel& operator = (TransactionsViewModel&&) = delete;

   size_t itemsCount() const { return rootNode_->nbChildren(); }

   int columnCount(const QModelIndex &parent = QModelIndex()) const override;
   int rowCount(const QModelIndex &parent = QModelIndex()) const override;
   QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
   QModelIndex parent(const QModelIndex &child) const override;
   bool hasChildren(const QModelIndex& parent = QModelIndex()) const override;

   QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
   QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

   TransactionPtr getItem(const QModelIndex &) const;
   TransactionPtr getOldestItem() const { return oldestItem_; }
   TXNode *getNode(const QModelIndex &) const;

   void onLedgerEntries(const std::string &filter, uint32_t totalPages
      , uint32_t curPage, uint32_t curBlock, const std::vector<bs::TXEntry> &);
   void onZCsInvalidated(const std::vector<BinaryData>& txHashes);
   void onNewBlock(unsigned int topBlock);
   void onTXDetails(const std::vector<bs::sync::TXWalletDetails> &);
   size_t removeEntriesFor(const bs::sync::HDWalletData&);

signals:
   void needTXDetails(const std::vector<bs::sync::TXWallet> &, bool useCache, const bs::Address &);

private slots:
   void onNewItems(const std::vector<TXNode *> &);
   void onDelRows(std::vector<int> rows);
   void onItemConfirmed(const TransactionPtr);
   void onRefreshTxValidity();

private:
   void clear();
   std::shared_ptr<TransactionsViewItem> createTxItem(const bs::TXEntry &);

signals:
   void dataLoaded(int count);
   void initProgress(int start, int end);
   void updateProgress(int value);

public:
   enum class Columns
   {
      first,
      Date = first,
      Wallet,
      SendReceive,
      Address,
      Amount,
      Status,
      Flag,
//      MissedBlocks,
      Comment,
      TxHash,
      last = TxHash
   };

   enum Role {
      SortRole=Qt::UserRole,
      FilterRole,
      WalletRole
   };

private:
   std::unique_ptr<TXNode>       rootNode_;
   TransactionPtr oldestItem_;
   std::shared_ptr<spdlog::logger>     logger_;
   mutable QMutex                      updateMutex_;
   std::shared_ptr<bs::sync::Wallet>   defaultWallet_;
   std::atomic_bool  signalOnEndLoading_{ false };
   std::shared_ptr<std::atomic_bool>  stopped_;
   std::atomic_bool  initialLoadCompleted_{ true };

   struct ItemKey {
      BinaryData  txHash;
      std::string walletId;

      bool operator<(const ItemKey &other) const
      {
         return ((txHash < other.txHash) || ((txHash == other.txHash)
            && (walletId < other.walletId)));
      }
      bool operator==(const ItemKey &other) const
      {
         return ((txHash == other.txHash) && (walletId == other.walletId));
      }
   };
   std::map<ItemKey, int>     itemIndex_;
   std::map<ItemKey, TXNode*> invalidatedNodes_;

   // If set, amount field will show only related address balance changes
   // (without fees because fees are related to transaction, not address).
   // Right now used with AddressDetailDialog only.
   // See BST-1982 and BST-1983 for details.
   const bs::Address filterAddress_;

   // Tx that could be revoked
   std::map<BinaryData, bool> revocableTxs_;

   uint32_t curBlock_{ 0 };
};

#endif // __TRANSACTIONS_VIEW_MODEL_H__
