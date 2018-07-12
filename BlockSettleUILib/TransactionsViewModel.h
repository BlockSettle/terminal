#ifndef __TRANSACTIONS_VIEW_MODEL_H__
#define __TRANSACTIONS_VIEW_MODEL_H__

#include <unordered_set>
#include <QAbstractItemModel>
#include <QMutex>
#include <QThreadPool>
#include <QColor>
#include <QFont>
#include <atomic>
#include "MetaData.h"


class WalletsManager;
class PyBlockDataManager;
class SafeLedgerDelegate;

struct TransactionsViewItem
{
   std::shared_ptr<ClientClasses::LedgerEntry> led;
   Tx tx;
   bool initialized = false;
   QString mainAddress;
   bs::Transaction::Direction direction = bs::Transaction::Unknown;
   std::shared_ptr<bs::Wallet> wallet = nullptr;
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

   void initialize(const std::shared_ptr<PyBlockDataManager> &, const std::shared_ptr<WalletsManager> &);
   void calcAmount(const std::shared_ptr<PyBlockDataManager> &, const std::shared_ptr<WalletsManager> &);
   bool containsInputsFrom(const Tx &tx) const;
};
typedef std::vector<TransactionsViewItem>    TransactionItems;

Q_DECLARE_METATYPE(TransactionsViewItem)
Q_DECLARE_METATYPE(TransactionItems)


class TransactionsViewModel : public QAbstractTableModel
{
Q_OBJECT
public:
    TransactionsViewModel(std::shared_ptr< PyBlockDataManager > bdm, const std::shared_ptr< WalletsManager >& walletsManager
       , const std::shared_ptr<SafeLedgerDelegate>& ledgerDelegate, QObject* parent, const std::shared_ptr<bs::Wallet> &defWlt = nullptr);
   ~TransactionsViewModel() noexcept;

   TransactionsViewModel(const TransactionsViewModel&) = delete;
   TransactionsViewModel& operator = (const TransactionsViewModel&) = delete;
   TransactionsViewModel(TransactionsViewModel&&) = delete;
   TransactionsViewModel& operator = (TransactionsViewModel&&) = delete;

public:
   int columnCount(const QModelIndex &parent = QModelIndex()) const override;
   int rowCount(const QModelIndex &parent = QModelIndex()) const override;

   QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
   QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
   bool isTransactionVerified(int transactionRow) const;

   TransactionsViewItem getItem(int transactionRow) const;
   std::shared_ptr<WalletsManager> GetWalletsManager() const;
   std::shared_ptr<PyBlockDataManager> GetBlockDataManager() const;

private slots:
   void updatePage();
   void refresh();
   void onZeroConf(std::vector<ClientClasses::LedgerEntry> page);
   void onRowUpdated(int index, TransactionsViewItem item, int colStart, int colEnd);
   void onNewItems(const TransactionItems items);
   void onItemsDeleted(const TransactionItems items);
   void onRawDataLoaded();
   void onDataLoaded();
   void onItemConfirmed(const TransactionsViewItem item);

   void onArmoryOffline() { clear(); }

private:
   void clear();
   void loadLedgerEntries();
   void ledgerToTxData();
   void loadNewTransactions();
   void insertNewTransactions(const std::vector<ClientClasses::LedgerEntry> &page);
   void loadTransactionDetails(unsigned int iStart, size_t count);
   void updateBlockHeight(const std::vector<ClientClasses::LedgerEntry> &page);
   void updateTransactionDetails(TransactionsViewItem &item, int index);
   TransactionsViewItem itemFromTransaction(const ClientClasses::LedgerEntry &);
   bool txKeyExists(const std::string &key);
   int getItemIndex(const TransactionsViewItem &) const;

signals:
   void dataChangedInThread(const QModelIndex& start, const QModelIndex& end, const QVector<int> roles = QVector<int>());
   void rowUpdated(int index, TransactionsViewItem item, int colStart, int colEnd);
   void itemsAdded(const TransactionItems items);
   void itemsDeleted(const TransactionItems items);
   void dataLoaded(int count);
   void itemConfirmed(const TransactionsViewItem item);

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
      RbfFlag,
//      MissedBlocks,
      Comment,
      last
   };

   enum Role {
      SortRole=Qt::UserRole,
      FilterRole
   };

   TransactionItems                    currentPage_;
   std::vector<ClientClasses::LedgerEntry>   rawData_;
   std::unordered_set<std::string>     currentKeys_;
   std::shared_ptr<PyBlockDataManager> bdm_;
   std::shared_ptr<SafeLedgerDelegate> ledgerDelegate_;
   std::shared_ptr<WalletsManager>     walletsManager_;
   std::atomic_bool                    updateRunning_;
   mutable QMutex                      updateMutex_;
   QThreadPool                         threadPool_;
   std::shared_ptr<bs::Wallet>         defaultWallet_;
   std::atomic_bool                    stopped_, refreshing_;
   QFont                               fontBold_;
   QColor                              colorGray_, colorRed_, colorYellow_, colorGreen_, colorInvalid_;
   int       updRowFirst_ = -1, updRowLast_ = 0;
   std::atomic_bool                    initialLoadCompleted_;
};

#endif // __TRANSACTIONS_VIEW_MODEL_H__
