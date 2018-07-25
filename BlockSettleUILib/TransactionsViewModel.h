#ifndef __TRANSACTIONS_VIEW_MODEL_H__
#define __TRANSACTIONS_VIEW_MODEL_H__

#include <unordered_set>
#include <QAbstractItemModel>
#include <QMutex>
#include <QThreadPool>
#include <QColor>
#include <QFont>
#include <QMetaType>
#include <atomic>
#include "ArmoryConnection.h"
#include "AsyncClient.h"
#include "MetaData.h"


class SafeLedgerDelegate;
class WalletsManager;

struct TransactionsViewItem
{
   std::shared_ptr<ClientClasses::LedgerEntry> led;
   Tx tx;
   std::map<BinaryData, Tx>   txIns;
   std::set<BinaryData>       txHashes;
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

   void initialize(const std::shared_ptr<ArmoryConnection> &
      , const std::shared_ptr<WalletsManager> &, std::function<void()>);
   void calcAmount(const std::shared_ptr<WalletsManager> &);
   bool containsInputsFrom(const Tx &tx) const;
};
typedef std::vector<TransactionsViewItem>    TransactionItems;

Q_DECLARE_METATYPE(TransactionsViewItem)
Q_DECLARE_METATYPE(TransactionItems)


class TransactionsViewModel : public QAbstractTableModel
{
Q_OBJECT
public:
    TransactionsViewModel(const std::shared_ptr<ArmoryConnection> &, const std::shared_ptr<WalletsManager> &
       , const AsyncClient::LedgerDelegate &, QObject* parent, const std::shared_ptr<bs::Wallet> &defWlt = nullptr);
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

private slots:
   void updatePage();
   void refresh();
   void onZeroConf(ArmoryConnection::ReqIdType);
   void onRowUpdated(int index, const TransactionsViewItem &item, int colStart, int colEnd);
   void onNewItems(const TransactionItems items);
   void onItemsDeleted(const TransactionItems items);
   void onItemConfirmed(const TransactionsViewItem item);

   void onArmoryStateChanged(ArmoryConnection::State);
   void onNewTransactions(std::vector<ClientClasses::LedgerEntry>);

private:
   void clear();
   void loadLedgerEntries();
   void ledgerToTxData();
   void insertNewTransactions(const std::vector<ClientClasses::LedgerEntry> &page);
   void loadTransactionDetails(unsigned int iStart, size_t count);
   void updateBlockHeight(const std::vector<ClientClasses::LedgerEntry> &page);
   void updateTransactionDetails(TransactionsViewItem &item, int index);
   TransactionsViewItem itemFromTransaction(const ClientClasses::LedgerEntry &);
   bool txKeyExists(const std::string &key);
   int getItemIndex(const TransactionsViewItem &) const;

signals:
   void dataChangedInThread(const QModelIndex& start, const QModelIndex& end, const QVector<int> roles = QVector<int>());
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
      FilterRole,
      WalletRole
   };

   TransactionItems                    currentPage_;
   std::map<uint32_t, std::vector<ClientClasses::LedgerEntry>> rawData_;
   std::unordered_set<std::string>     currentKeys_;
   std::shared_ptr<ArmoryConnection>   armory_;
   AsyncClient::LedgerDelegate         ledgerDelegate_;
   std::shared_ptr<WalletsManager>     walletsManager_;
   std::atomic_bool                    updateRunning_;
   mutable QMutex                      updateMutex_;
   std::shared_ptr<bs::Wallet>         defaultWallet_;
   std::atomic_bool  stopped_;
   QFont             fontBold_;
   QColor            colorGray_, colorRed_, colorYellow_, colorGreen_, colorInvalid_;
   std::atomic_bool  initialLoadCompleted_;
};

#endif // __TRANSACTIONS_VIEW_MODEL_H__
