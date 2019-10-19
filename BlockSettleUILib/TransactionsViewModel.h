#ifndef __TRANSACTIONS_VIEW_MODEL_H__
#define __TRANSACTIONS_VIEW_MODEL_H__

#include <deque>
#include <unordered_set>
#include <QAbstractItemModel>
#include <QMutex>
#include <QThreadPool>
#include <QColor>
#include <QFont>
#include <QMetaType>
#include <QTimer>
#include <atomic>
#include "ArmoryConnection.h"
#include "AsyncClient.h"
#include "Wallets/SyncWallet.h"

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
   bool initialized = false;
   QString mainAddress;
   int addressCount;
   bs::sync::Transaction::Direction direction = bs::sync::Transaction::Unknown;
   std::shared_ptr<bs::sync::Wallet> wallet = nullptr;
   QString dirStr;
   QString walletName;
   QString walletID;
   QString comment;
   QString displayDateTime;
   QString amountStr;
   BTCNumericTypes::balance_type amount = 0;
   bool     isValid = true;
   bool     isCPFP = false;
   int confirmations = 0;

   BinaryData  parentId;   // universal grouping support
   BinaryData  groupId;

   bool isSet() const { return (!txEntry.txHash.isNull() && !walletID.isEmpty()); }
   static void initialize(const TransactionPtr &item, ArmoryConnection *
      , const std::shared_ptr<bs::sync::WalletsManager> &
      , std::function<void(const TransactionPtr &)>);
   void calcAmount(const std::shared_ptr<bs::sync::WalletsManager> &);
   bool containsInputsFrom(const Tx &tx) const;

   bool isRBFeligible() const;
   bool isCPFPeligible() const;

   std::string id() const;
   bs::Address filterAddress;

private:
   bool     txHashesReceived{ false };
   std::map<BinaryData, Tx>   txIns;
   mutable std::string        id_;
};
typedef std::vector<TransactionsViewItem>    TransactionItems;

class TXNode
{
public:
   TXNode();
   TXNode(const std::shared_ptr<TransactionsViewItem> &, TXNode *parent = nullptr);
   ~TXNode() { clear(); }

   std::shared_ptr<TransactionsViewItem> item() const { return item_; }
   size_t nbChildren() const { return children_.size(); }
   bool hasChildren() const { return !children_.empty(); }
   TXNode *child(int index) const;
   QList<TXNode *> children() const { return children_; }
   TXNode *parent() const { return parent_; }
   TXNode *find(const std::string &id) const;

   void clear(bool del = true);
   void setData(const TransactionsViewItem &data) { *item_ = data; }
   void add(TXNode *child);
   void del(int index);
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
   QColor   colorGray_, colorRed_, colorYellow_, colorGreen_, colorInvalid_;
};

Q_DECLARE_METATYPE(TransactionsViewItem)
Q_DECLARE_METATYPE(TransactionItems)


class TransactionsViewModel : public QAbstractItemModel, public ArmoryCallbackTarget
{
Q_OBJECT
public:
   TransactionsViewModel(const std::shared_ptr<ArmoryConnection> &
                          , const std::shared_ptr<bs::sync::WalletsManager> &
                          , const std::shared_ptr<AsyncClient::LedgerDelegate> &
                          , const std::shared_ptr<spdlog::logger> &
                          , const std::shared_ptr<bs::sync::Wallet> &defWlt
                          , const bs::Address &filterAddress = bs::Address()
                          , QObject* parent = nullptr);
   TransactionsViewModel(const std::shared_ptr<ArmoryConnection> &
                          , const std::shared_ptr<bs::sync::WalletsManager> &
                          , const std::shared_ptr<spdlog::logger> &
                          , QObject* parent = nullptr);
   ~TransactionsViewModel() noexcept override;

   TransactionsViewModel(const TransactionsViewModel&) = delete;
   TransactionsViewModel& operator = (const TransactionsViewModel&) = delete;
   TransactionsViewModel(TransactionsViewModel&&) = delete;
   TransactionsViewModel& operator = (TransactionsViewModel&&) = delete;

   void loadAllWallets(bool onNewBlock=false);
   size_t itemsCount() const { return currentItems_.size(); }

public:
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

private slots:
   void updatePage();
   void refresh();
   void onWalletDeleted(std::string walletId);
   void onNewItems(const std::unordered_map<std::string, std::pair<TransactionPtr, TXNode *>> &);
   void onDelRows(std::vector<int> rows);

   void onItemConfirmed(const TransactionPtr);

private:
   void onNewBlock(unsigned int height, unsigned int branchHgt) override;
   void onStateChanged(ArmoryState) override;
   void onZCReceived(const std::vector<bs::TXEntry> &) override;
   void onZCInvalidated(const std::vector<bs::TXEntry> &) override;

   void init();
   void clear();
   void loadLedgerEntries(bool onNewBlock=false);
   void ledgerToTxData(const std::map<int, std::vector<bs::TXEntry>> &rawData
      , bool onNewBlock=false);
   std::pair<size_t, size_t> updateTransactionsPage(const std::vector<bs::TXEntry> &);
   void updateBlockHeight(const std::vector<std::shared_ptr<TransactionsViewItem>> &);
   void updateTransactionDetails(const TransactionPtr &item
      , const std::function<void(const TransactionPtr &)> &cb);
   std::shared_ptr<TransactionsViewItem> itemFromTransaction(const bs::TXEntry &);
   std::shared_ptr<TransactionsViewItem> getTxEntry(const std::string &key);

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
      last
   };

   enum Role {
      SortRole=Qt::UserRole,
      FilterRole,
      WalletRole
   };

private:
   std::unique_ptr<TXNode> rootNode_;
   TransactionPtr oldestItem_;
   std::unordered_map<std::string, TransactionPtr> currentItems_;
   std::shared_ptr<spdlog::logger>     logger_;
   std::shared_ptr<AsyncClient::LedgerDelegate> ledgerDelegate_;
   std::shared_ptr<bs::sync::WalletsManager>    walletsManager_;
   mutable QMutex                      updateMutex_;
   std::shared_ptr<bs::sync::Wallet>   defaultWallet_;
   std::atomic_bool  signalOnEndLoading_{ false };
   const bool        allWallets_;
   std::shared_ptr<std::atomic_bool>  stopped_;
   std::atomic_bool  initialLoadCompleted_{ true };

   // If set, amount field will show only related address balance changes
   // (without fees because fees are related to transaction, not address).
   // Right now used with AddressDetailDialog only.
   // See BST-1982 and BST-1983 for details.
   const bs::Address filterAddress_;
};

#endif // __TRANSACTIONS_VIEW_MODEL_H__
