#include "TransactionsWidget.h"

#include <QSortFilterProxyModel>
#include <QMenu>
#include <QClipboard>
#include <QDateTime>

#include "ui_TransactionsWidget.h"

#include "CreateTransactionDialogAdvanced.h"
#include "HDWallet.h"
#include "BSMessageBox.h"
#include "TransactionsViewModel.h"
#include "TransactionDetailDialog.h"
#include "WalletsManager.h"
#include "UiUtils.h"
#include "ApplicationSettings.h"

static const QString c_allWalletsId = QLatin1String("all");


class TransactionsSortFilterModel : public QSortFilterProxyModel
{
public:
   TransactionsSortFilterModel(std::shared_ptr<ApplicationSettings> &appSettings, QObject* parent)
      : QSortFilterProxyModel(parent)
      , appSettings_(appSettings)
   {
      setSortRole(TransactionsViewModel::SortRole);
   }

/*   int rowCount(const QModelIndex & parent = QModelIndex()) const override
   {     //! causes assert(last < rowCount()) to invoke when filtering by wallet
      return qMin(QSortFilterProxyModel::rowCount(parent), 500);
   }*/

   int totalRowCount() const
   {
      return QSortFilterProxyModel::rowCount();
   }

   bool filterAcceptsRow(int source_row, const QModelIndex &) const override
   {
      auto src = sourceModel();
      if (!src) {
         return false;
      }
      QModelIndex directionIndex = src->index(source_row, static_cast<int>(TransactionsViewModel::Columns::SendReceive));
      int direction = src->data(directionIndex, TransactionsViewModel::FilterRole).toInt();

      bool walletMatched = false;
      if (!walletIds.isEmpty()) {
         const QModelIndex index = src->index(source_row,
            static_cast<int>(TransactionsViewModel::Columns::Wallet));
         for (const auto &walletId : walletIds) {
            if (src->data(index, TransactionsViewModel::FilterRole).toString() == walletId) {
               walletMatched = true;
            }
         }
         if (!walletMatched) {
            return false;
         }
      }

      if (transactionDirection != bs::Transaction::Unknown) {
         const auto aIdx = src->index(source_row,
            static_cast<int>(TransactionsViewModel::Columns::Amount));
         const auto wallet = static_cast<bs::Wallet*>(aIdx.data(
            TransactionsViewModel::WalletRole).value<void*>());

         if (!walletIds.isEmpty() && wallet->GetType() == bs::wallet::Type::ColorCoin) {
            const auto a = aIdx.data(Qt::DisplayRole).toDouble();

            switch (transactionDirection) {
               case bs::Transaction::Received : {
                  if (a < 0.0) {
                     return false;
                  }
               }
                  break;

               case bs::Transaction::Sent : {
                  if (a > 0.0) {
                     return false;
                  }
               }
                  break;

               default :
                  return false;
            }
         } else if (direction != transactionDirection) {
            return false;
         }
      }

      bool result = true;

      if ((startDate > 0) && (endDate > 0)) {
         QModelIndex index = src->index(source_row, static_cast<int>(TransactionsViewModel::Columns::Date));
         uint32_t txDate = src->data(index, TransactionsViewModel::FilterRole).toUInt();
         result = (startDate <= txDate) && (txDate <= endDate);
      }

      if (result && !searchString.isEmpty()) {     // more columns can be added later
         for (const auto &col : { TransactionsViewModel::Columns::Comment, TransactionsViewModel::Columns::Address }) {
            QModelIndex index = src->index(source_row, static_cast<int>(col));
            if (src->data(index, TransactionsViewModel::FilterRole).toString().contains(searchString, Qt::CaseInsensitive)) {
               return true;
            }
         }
         return false;
      }

      return result;
   }

   bool filterAcceptsColumn(int source_column, const QModelIndex &source_parent) const override
   {
      Q_UNUSED(source_parent);
/*      const auto col = static_cast<TransactionsViewModel::Columns>(source_column);
      return (col != TransactionsViewModel::Columns::MissedBlocks);*/
      return true;   // strange, but it works properly only this way
   }

   void updateFilters(const QStringList &walletIds, const QString &searchString, bs::Transaction::Direction direction)
   {
      this->walletIds = walletIds;
      this->searchString = searchString;
      this->transactionDirection = direction;

      appSettings_->set(ApplicationSettings::TransactionFilter,
         QVariantList() << (this->walletIds.isEmpty() ?
            QStringList() << c_allWalletsId : this->walletIds) <<
         static_cast<int>(direction));

      invalidateFilter();
   }

   void updateDates(const QDateTime& start, const QDateTime& end)
   {
      this->startDate = start.isValid() ? start.toTime_t() : 0;
      this->endDate = end.isValid() ? end.toTime_t() : 0;
      invalidateFilter();
   }

   std::shared_ptr<ApplicationSettings> appSettings_;
   QStringList walletIds;
   QString searchString;
   bs::Transaction::Direction transactionDirection = bs::Transaction::Unknown;
   uint32_t startDate = 0;
   uint32_t endDate = 0;
};


TransactionsWidget::TransactionsWidget(QWidget* parent)
   : TabWithShortcut(parent)
   , ui(new Ui::TransactionsWidget())
   , transactionsModel_(nullptr)
   , sortFilterModel_(nullptr)

{
   ui->setupUi(this);
   connect(ui->treeViewTransactions, &QAbstractItemView::doubleClicked, this, &TransactionsWidget::showTransactionDetails);
   ui->treeViewTransactions->setContextMenuPolicy(Qt::CustomContextMenu);

   actionCopyAddr_ = new QAction(tr("&Copy Address"));
   connect(actionCopyAddr_, &QAction::triggered, [this]() {
      qApp->clipboard()->setText(curAddress_);
   });

   actionCopyTx_ = new QAction(tr("Copy &Transaction Hash"));
   connect(actionCopyTx_, &QAction::triggered, [this]() {
      qApp->clipboard()->setText(curTx_);
   });

   actionRBF_ = new QAction(tr("Replace-By-Fee (RBF)"), this);
   connect(actionRBF_, &QAction::triggered, this, &TransactionsWidget::onCreateRBFDialog);

   actionCPFP_ = new QAction(tr("Child-Pays-For-Parent (CPFP)"), this);
   connect(actionCPFP_, &QAction::triggered, this, &TransactionsWidget::onCreateCPFPDialog);

   connect(ui->treeViewTransactions, &QAbstractItemView::customContextMenuRequested, [=](const QPoint& p) {
      auto index = sortFilterModel_->mapToSource(ui->treeViewTransactions->indexAt(p));
      auto addressIndex = transactionsModel_->index(index.row(), static_cast<int>(TransactionsViewModel::Columns::Address));
      curAddress_ = transactionsModel_->data(addressIndex).toString();

      contextMenu_.clear();

      if (sortFilterModel_) {
         const auto &sourceIndex = sortFilterModel_->mapToSource(ui->treeViewTransactions->indexAt(p));
         const auto &txNode = transactionsModel_->getNode(sourceIndex);
         if (txNode && txNode->item() && txNode->item()->initialized) {
            if (txNode->item()->isRBFeligible() && (txNode->level() < 2)) {
               contextMenu_.addAction(actionRBF_);
               actionRBF_->setData(sourceIndex);
            }
            else {
               actionRBF_->setData(-1);
            }

            if (txNode->item()->isCPFPeligible()) {
               contextMenu_.addAction(actionCPFP_);
               actionCPFP_->setData(sourceIndex);
            }
            else {
               actionCPFP_->setData(-1);
            }

            // save transaction id and add context menu for copying it to clipboard
            curTx_ = QString::fromStdString(txNode->item()->txEntry.txHash.toHexStr(true));
            contextMenu_.addAction(actionCopyTx_);

            // allow copy address only if there is only 1 address
            if (txNode->item()->addressCount == 1) {
               contextMenu_.addAction(actionCopyAddr_);
            }
         }
      }
      contextMenu_.popup(ui->treeViewTransactions->mapToGlobal(p));
   });
   ui->treeViewTransactions->setUniformRowHeights(true);
   ui->treeViewTransactions->setItemsExpandable(true);
   ui->treeViewTransactions->setRootIsDecorated(true);
   ui->treeViewTransactions->header()->setSectionResizeMode(QHeaderView::ResizeToContents);

   connect(ui->typeFilterComboBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), [&](int index) {
      sortFilterModel_->updateFilters(sortFilterModel_->walletIds, sortFilterModel_->searchString, static_cast<bs::Transaction::Direction>(index));
   });

   connect(ui->walletBox, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, &TransactionsWidget::walletsFilterChanged);

   ui->dateEditEnd->setDateTime(QDateTime::currentDateTime().addDays(1));

   connect(ui->dateEditEnd, &QDateTimeEdit::dateTimeChanged, [=](const QDateTime& dateTime) {
      if (ui->dateEditStart->dateTime() > dateTime)
      {
         ui->dateEditStart->setDateTime(dateTime);
      }
   });
   connect(ui->dateEditStart, &QDateTimeEdit::dateTimeChanged, [=](const QDateTime& dateTime) {
      if (ui->dateEditEnd->dateTime() < dateTime)
      {
         ui->dateEditEnd->setDateTime(dateTime);
      }
   });

   connect(ui->treeViewTransactions, &TreeViewWithEnterKey::enterKeyPressed,
          this, &TransactionsWidget::onEnterKeyInTrxPressed);

   ui->labelResultCount->hide();
}

TransactionsWidget::~TransactionsWidget() = default;

void TransactionsWidget::init(const std::shared_ptr<WalletsManager> &walletsMgr
                              , const std::shared_ptr<ArmoryConnection> &armory
                              , const std::shared_ptr<SignContainer> &signContainer
                              , const std::shared_ptr<spdlog::logger> &logger)

{
   walletsManager_ = walletsMgr;
   armory_ = armory;
   signContainer_ = signContainer;
   logger_ = logger;

   connect(walletsManager_.get(), &WalletsManager::walletChanged, this, &TransactionsWidget::walletsChanged);
}

void TransactionsWidget::SetTransactionsModel(const std::shared_ptr<TransactionsViewModel>& model)
{
   transactionsModel_ = model;
   connect(transactionsModel_.get(), &TransactionsViewModel::dataLoaded, this, &TransactionsWidget::onDataLoaded, Qt::QueuedConnection);
   connect(transactionsModel_.get(), &TransactionsViewModel::initProgress, this, &TransactionsWidget::onProgressInited);
   connect(transactionsModel_.get(), &TransactionsViewModel::updateProgress, this, &TransactionsWidget::onProgressUpdated);

   sortFilterModel_ = new TransactionsSortFilterModel(appSettings_, this);
   sortFilterModel_->setSourceModel(model.get());

   connect(sortFilterModel_, &TransactionsSortFilterModel::rowsInserted, this, &TransactionsWidget::updateResultCount);
   connect(sortFilterModel_, &TransactionsSortFilterModel::rowsRemoved, this, &TransactionsWidget::updateResultCount);
   connect(sortFilterModel_, &TransactionsSortFilterModel::modelReset, this, &TransactionsWidget::updateResultCount);

   walletsChanged();

   auto updateDateTimes = [=]() {
      sortFilterModel_->updateDates(ui->dateEditStart->dateTime(), ui->dateEditEnd->dateTime());
   };
   connect(ui->dateEditStart, &QDateTimeEdit::dateTimeChanged, updateDateTimes);
   connect(ui->dateEditEnd, &QDateTimeEdit::dateTimeChanged, updateDateTimes);

   connect(ui->searchField, &QLineEdit::textChanged, [=](const QString& text) {
      sortFilterModel_->updateFilters(sortFilterModel_->walletIds, text, sortFilterModel_->transactionDirection);
   });

   ui->treeViewTransactions->setSortingEnabled(true);
   ui->treeViewTransactions->setModel(sortFilterModel_);
   ui->treeViewTransactions->hideColumn(static_cast<int>(TransactionsViewModel::Columns::TxHash));
//   ui->treeViewTransactions->hideColumn(static_cast<int>(TransactionsViewModel::Columns::MissedBlocks));
}

void TransactionsWidget::onDataLoaded(int count)
{
   ui->progressBar->hide();
   ui->progressBar->setMaximum(0);
   ui->progressBar->setMinimum(0);

   if ((count <= 0) || (ui->dateEditStart->dateTime().date().year() > 2009)) {
      return;
   }
   const auto &item = transactionsModel_->getOldestItem();
   ui->dateEditStart->setDateTime(QDateTime::fromTime_t(item.txEntry.txTime));
}

void TransactionsWidget::onProgressInited(int start, int end)
{
   ui->progressBar->setMinimum(start);
   ui->progressBar->setMaximum(end);
}

void TransactionsWidget::onProgressUpdated(int value)
{
   ui->progressBar->setValue(value);
}

void TransactionsWidget::setAppSettings(std::shared_ptr<ApplicationSettings> appSettings)
{
   appSettings_ = appSettings;
}

void TransactionsWidget::shortcutActivated(ShortcutType s)
{
   if (s == ShortcutType::Alt_1)
      ui->treeViewTransactions->activate();
}

static inline QStringList walletLeavesIds(WalletsManager::hd_wallet_type wallet)
{
   QStringList allLeafIds;

   for (const auto &leaf : wallet->getLeaves()) {
      const QString id = QString::fromStdString(leaf->GetWalletId());
      allLeafIds << id;
   }

   return allLeafIds;
}

static inline bool exactlyThisLeaf(const QStringList &ids, const QStringList &walletIds)
{
   if (ids.size() != walletIds.size()) {
      return false;
   }

   int count = 0;

   count = std::accumulate(ids.cbegin(), ids.cend(), count,
      [&](int value, const QString &id) {
         if (walletIds.contains(id)) {
            return ++value;
         } else {
            return value;
         }
   });

   return (count == walletIds.size());
}

void TransactionsWidget::walletsChanged()
{
   QStringList walletIds;
   int direction;

   const auto varList = appSettings_->get(ApplicationSettings::TransactionFilter).toList();
   walletIds = varList.first().toStringList();
   direction = varList.last().toInt();

   int currentIndex = -1;
   int primaryWalletIndex = 0;

   ui->walletBox->clear();
   ui->walletBox->addItem(tr("All Wallets"));
   int index = 1;
   for (unsigned int i = 0; i < walletsManager_->GetHDWalletsCount(); i++) {
      const auto &hdWallet = walletsManager_->GetHDWallet(i);
      ui->walletBox->addItem(QString::fromStdString(hdWallet->getName()));
      QStringList allLeafIds = walletLeavesIds(hdWallet);

      if (exactlyThisLeaf(walletIds, allLeafIds)) {
         currentIndex = index;
      }

      if (hdWallet == walletsManager_->GetPrimaryWallet()) {
         primaryWalletIndex = index;
      }

      ui->walletBox->setItemData(index++, allLeafIds, UiUtils::WalletIdRole);

      for (const auto &group : hdWallet->getGroups()) {
         ui->walletBox->addItem(QString::fromStdString("   " + group->getName()));
         const auto groupIndex = index++;
         QStringList groupLeafIds;
         for (const auto &leaf : group->getLeaves()) {
            groupLeafIds << QString::fromStdString(leaf->GetWalletId());
            ui->walletBox->addItem(QString::fromStdString("      " + leaf->GetShortName()));

            const auto id = QString::fromStdString(leaf->GetWalletId());
            QStringList ids;
            ids << id;

            ui->walletBox->setItemData(index, ids, UiUtils::WalletIdRole);

            if (exactlyThisLeaf(walletIds, ids)) {
               currentIndex = index;
            }

            index++;
         }
         if (groupLeafIds.isEmpty()) {
            groupLeafIds << QLatin1String("non-existent");
         }
         ui->walletBox->setItemData(groupIndex, groupLeafIds, UiUtils::WalletIdRole);
      }
   }
   const auto &settlWallet = walletsManager_->GetSettlementWallet();
   if (settlWallet) {
      ui->walletBox->addItem(QLatin1String("Settlement"));
      ui->walletBox->setItemData(index++, QStringList() << QString::fromStdString(settlWallet->GetWalletId())
         , UiUtils::WalletIdRole);
   }

   ui->typeFilterComboBox->setCurrentIndex(direction);

   if (currentIndex >= 0) {
      ui->walletBox->setCurrentIndex(currentIndex);
   } else {
      if (walletIds.contains(c_allWalletsId)) {
         ui->walletBox->setCurrentIndex(0);
      } else {
         const auto primaryWallet = walletsManager_->GetPrimaryWallet();

         if (primaryWallet) {
            ui->walletBox->setCurrentIndex(primaryWalletIndex);
         } else {
            ui->walletBox->setCurrentIndex(0);
         }
      }
   }
}

void TransactionsWidget::walletsFilterChanged(int index)
{
   if (index < 0) {
      return;
   }
   const auto &walletIds = ui->walletBox->itemData(index, UiUtils::WalletIdRole).toStringList();
   sortFilterModel_->updateFilters(walletIds, sortFilterModel_->searchString, sortFilterModel_->transactionDirection);
}

void TransactionsWidget::onEnterKeyInTrxPressed(const QModelIndex &index)
{
   showTransactionDetails(index);
}

void TransactionsWidget::showTransactionDetails(const QModelIndex& index)
{
   auto txItem = transactionsModel_->getItem(sortFilterModel_->mapToSource(index));

   TransactionDetailDialog transactionDetailDialog(txItem, walletsManager_, armory_, this);
   transactionDetailDialog.exec();
}

void TransactionsWidget::updateResultCount()
{
   auto shown = sortFilterModel_->rowCount();
   auto total = transactionsModel_->itemsCount();
   ui->labelResultCount->setText(tr("Displaying %L1 transactions (of %L2 total).")
      .arg(shown).arg(total));
   ui->labelResultCount->show();
}

void TransactionsWidget::onCreateRBFDialog()
{
   auto txItem = transactionsModel_->getItem(actionRBF_->data().toModelIndex());

   const auto &cbDialog = [this](const TransactionsViewItem *txItem) {
      try {
         auto dlg = CreateTransactionDialogAdvanced::CreateForRBF(armory_
            , walletsManager_, signContainer_, logger_, txItem->tx
            , txItem->wallet, this);
         dlg->exec();
      }
      catch (const std::exception &e) {
         BSMessageBox(BSMessageBox::critical, tr("RBF Transaction"), tr("Failed to create RBF transaction")
            , QLatin1String(e.what()), this).exec();
      }
   };

   if (txItem.initialized) {
      cbDialog(&txItem);
   }
   else {
      txItem.initialize(armory_, walletsManager_, cbDialog);
   }
}

void TransactionsWidget::onCreateCPFPDialog()
{
   auto txItem = transactionsModel_->getItem(actionCPFP_->data().toModelIndex());

   const auto &cbDialog = [this](const TransactionsViewItem *txItem) {
      try {
         auto dlg = CreateTransactionDialogAdvanced::CreateForCPFP(armory_
            , walletsManager_, signContainer_, txItem->wallet
            , logger_, txItem->tx, this);
         dlg->exec();
      }
      catch (const std::exception &e) {
         BSMessageBox(BSMessageBox::critical, tr("CPFP Transaction"), tr("Failed to create CPFP transaction")
            , QLatin1String(e.what()), this).exec();
      }
   };

   if (txItem.initialized) {
      cbDialog(&txItem);
   }
   else {
      txItem.initialize(armory_, walletsManager_, cbDialog);
   }
}
