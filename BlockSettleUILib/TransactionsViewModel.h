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
#include "MetaData.h"
#include "WalletsManager.h"

namespace spdlog {
   class logger;
}

class SafeLedgerDelegate;
class WalletsManager;

struct TransactionsViewItem
{
   bs::TXEntry txEntry;
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

   void initialize(const std::shared_ptr<ArmoryConnection> &
      , const std::shared_ptr<WalletsManager> &, std::function<void(const TransactionsViewItem *)>);
   void calcAmount(const std::shared_ptr<WalletsManager> &);
   bool containsInputsFrom(const Tx &tx) const;

   bool isRBFeligible() const;
   bool isCPFPeligible() const;
private:
   std::set<BinaryData>       txHashes;
   std::map<BinaryData, Tx>   txIns;

};
typedef std::vector<TransactionsViewItem>    TransactionItems;

Q_DECLARE_METATYPE(TransactionsViewItem)
Q_DECLARE_METATYPE(TransactionItems)


class TransactionsViewModel : public QAbstractTableModel
{
Q_OBJECT
public:
    TransactionsViewModel(const std::shared_ptr<ArmoryConnection> &
                          , const std::shared_ptr<WalletsManager> &
                          , const AsyncClient::LedgerDelegate &
                          , const std::shared_ptr<spdlog::logger> &
                          , QObject* parent
                          , const std::shared_ptr<bs::Wallet> &defWlt);
    TransactionsViewModel(const std::shared_ptr<ArmoryConnection> &
                          , const std::shared_ptr<WalletsManager> &
                          , const std::shared_ptr<spdlog::logger> &
                          , QObject* parent = nullptr);
    ~TransactionsViewModel() noexcept;

   TransactionsViewModel(const TransactionsViewModel&) = delete;
   TransactionsViewModel& operator = (const TransactionsViewModel&) = delete;
   TransactionsViewModel(TransactionsViewModel&&) = delete;
   TransactionsViewModel& operator = (TransactionsViewModel&&) = delete;

   void loadAllWallets();

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
   void onRowUpdated(int index, const TransactionsViewItem &item, int colStart, int colEnd);
   void onNewItems(TransactionItems items);
   void onItemsDeleted(TransactionItems items);

   void onArmoryStateChanged(ArmoryConnection::State);
   void onNewTransactions(std::vector<bs::TXEntry>);
   void timerCmd();

private:
   void init();
   void clear();
   Q_INVOKABLE void loadLedgerEntries();
   void ledgerToTxData();
   void loadTransactionDetails(unsigned int iStart, size_t count);
   void updateBlockHeight(const std::vector<bs::TXEntry> &page);
   void updateTransactionDetails(TransactionsViewItem &item, int index);
   void updateTransactionDetails(TransactionsViewItem &item
      , const std::function<void(const TransactionsViewItem *itemPtr)> &cb);
   TransactionsViewItem itemFromTransaction(const bs::TXEntry &);
   bool txKeyExists(const std::string &key);
   int getItemIndex(const TransactionsViewItem &) const;

signals:
   void dataLoaded(int count);

private:
   struct Command {
      enum class Type {
         Add,
         Delete,
         Update
      };
      Type  type;
      TransactionItems  items;
   };
   QTimer * cmdTimer_;
   QMutex   cmdMutex_;
   using CommandQueue = std::deque<Command>;
   CommandQueue cmdQueue_;

   void executeCommand(const Command &);
   void addCommand(const Command &);

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
      TxHash,
      last
   };

   enum Role {
      SortRole=Qt::UserRole,
      FilterRole,
      WalletRole
   };

private:
   TransactionItems                    currentPage_;
   TransactionItems                    erasedPage_;
   std::map<uint32_t, std::vector<bs::TXEntry>> rawData_;
   std::unordered_set<std::string>     currentKeys_;
   std::shared_ptr<ArmoryConnection>   armory_;
   std::shared_ptr<spdlog::logger>     logger_;
   AsyncClient::LedgerDelegate         ledgerDelegate_;
   std::shared_ptr<WalletsManager>     walletsManager_;
   mutable QMutex                      updateMutex_;
   std::shared_ptr<bs::Wallet>         defaultWallet_;
   std::vector<bs::TXEntry>            pendingNewItems_;
   const bool        allWallets_;
   std::atomic_bool  stopped_;
   QFont             fontBold_;
   QColor            colorGray_, colorRed_, colorYellow_, colorGreen_, colorInvalid_;
   std::atomic_bool  initialLoadCompleted_;
};

#endif // __TRANSACTIONS_VIEW_MODEL_H__
