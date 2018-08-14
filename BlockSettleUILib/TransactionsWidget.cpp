#include "TransactionsWidget.h"

#include <QSortFilterProxyModel>
#include <QMenu>
#include <QClipboard>
#include <QDateTime>

#include "ui_TransactionsWidget.h"

#include "HDWallet.h"
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

   int rowCount(const QModelIndex & parent = QModelIndex()) const override
   {
      return qMin(QSortFilterProxyModel::rowCount(parent), 500);
   }

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

      if (!searchString.isEmpty()) {
         QModelIndex index = src->index(source_row, static_cast<int>(TransactionsViewModel::Columns::Comment));
         if (!src->data(index, TransactionsViewModel::FilterRole).toString().contains(searchString, Qt::CaseInsensitive)) {
            return false;
         }
      }

      if ((startDate > 0) && (endDate > 0)) {
         QModelIndex index = src->index(source_row, static_cast<int>(TransactionsViewModel::Columns::Date));
         uint32_t txDate = src->data(index, TransactionsViewModel::FilterRole).toUInt();
         return (startDate <= txDate) && (txDate <= endDate);
      }

      return true;
   }

   bool filterAcceptsColumn(int source_column, const QModelIndex &source_parent) const override
   {
      Q_UNUSED(source_parent);
/*      const auto col = static_cast<TransactionsViewModel::Columns>(source_column);
      return (col != TransactionsViewModel::Columns::RbfFlag) && (col != TransactionsViewModel::Columns::MissedBlocks);*/
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
   connect(ui->treeViewTransactions, &QAbstractItemView::customContextMenuRequested, [=](const QPoint& p) {
      auto index = sortFilterModel_->mapToSource(ui->treeViewTransactions->indexAt(p));
      auto addressIndex = transactionsModel_->index(index.row(), static_cast<int>(TransactionsViewModel::Columns::Address));
      auto address = transactionsModel_->data(addressIndex).toString();

      QMenu* menu = new QMenu(this);
      QAction* copyAction = menu->addAction(tr("&Copy Address"));
      connect(copyAction, &QAction::triggered, [=]() {
         qApp->clipboard()->setText(address);
      });
      menu->popup(ui->treeViewTransactions->mapToGlobal(p));
   });
   ui->treeViewTransactions->setUniformRowHeights(true);
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

void TransactionsWidget::SetTransactionsModel(const std::shared_ptr<TransactionsViewModel>& model)
{
   transactionsModel_ = model;
   connect(transactionsModel_.get(), &TransactionsViewModel::dataLoaded, [this](int count) {
      if (count > 0) {
         auto index = transactionsModel_->index(count - 1, static_cast<int>(TransactionsViewModel::Columns::Date));
         auto dateTime = transactionsModel_->data(index).toDateTime();
         ui->dateEditStart->setDateTime(dateTime);
      }
   });

   sortFilterModel_ = new TransactionsSortFilterModel(appSettings_, this);
   sortFilterModel_->setSourceModel(model.get());

   connect(sortFilterModel_, &TransactionsSortFilterModel::rowsInserted, this, &TransactionsWidget::updateResultCount);
   connect(sortFilterModel_, &TransactionsSortFilterModel::rowsRemoved, this, &TransactionsWidget::updateResultCount);
   connect(sortFilterModel_, &TransactionsSortFilterModel::modelReset, this, &TransactionsWidget::updateResultCount);
   connect(transactionsModel_->GetWalletsManager().get(), &WalletsManager::walletChanged, this, &TransactionsWidget::walletsChanged);

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
   ui->treeViewTransactions->hideColumn(static_cast<int>(TransactionsViewModel::Columns::RbfFlag));
//   ui->treeViewTransactions->hideColumn(static_cast<int>(TransactionsViewModel::Columns::MissedBlocks));
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

static inline bool anyIdInThisWallet(const QStringList &ids, const QStringList &walletIds)
{
   for (const auto &id : qAsConst(ids)) {
      if (walletIds.contains(id)) {
         return true;
      }
   }

   return false;
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

   const auto &walletsManager = transactionsModel_->GetWalletsManager();
   ui->walletBox->clear();
   ui->walletBox->addItem(tr("All Wallets"));
   int index = 1;
   for (unsigned int i = 0; i < walletsManager->GetHDWalletsCount(); i++) {
      const auto &hdWallet = walletsManager->GetHDWallet(i);
      ui->walletBox->addItem(QString::fromStdString(hdWallet->getName()));
      QStringList allLeafIds = walletLeavesIds(hdWallet);

      if (anyIdInThisWallet(walletIds, allLeafIds)) {
         currentIndex = index;
      }

      if (hdWallet == walletsManager->GetPrimaryWallet()) {
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
            ui->walletBox->setItemData(index, QStringList() << QString::fromStdString(leaf->GetWalletId())
               , UiUtils::WalletIdRole);
            index++;
         }
         if (groupLeafIds.isEmpty()) {
            groupLeafIds << QLatin1String("non-existent");
         }
         ui->walletBox->setItemData(groupIndex, groupLeafIds, UiUtils::WalletIdRole);
      }
   }
   const auto &settlWallet = walletsManager->GetSettlementWallet();
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
         const auto primaryWallet = walletsManager->GetPrimaryWallet();

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
   auto txItem = transactionsModel_->getItem(sortFilterModel_->mapToSource(index).row());

   TransactionDetailDialog transactionDetailDialog(txItem, transactionsModel_->GetWalletsManager(), transactionsModel_->GetBlockDataManager(), this);
   transactionDetailDialog.exec();
}

void TransactionsWidget::updateResultCount()
{
   auto all = sortFilterModel_->totalRowCount();
   auto shown = sortFilterModel_->rowCount();
   auto total = transactionsModel_->rowCount();
   if (shown < total) {
      ui->labelResultCount->setText(tr("Showing %L1 of %L2 results (of %L3 total). To see other transactions, change the filters and/or date range above. ").arg(shown).arg(all).arg(total));
      ui->labelResultCount->show();
   }
   else if (all == 0 && total != 0) {
      ui->labelResultCount->setText(tr("None of the %L1 transactions match the filter criteria. To see other transactions, change the filters and/or date range above. ").arg(total));
      ui->labelResultCount->show();
   }
   else {
      ui->labelResultCount->hide();
   }
}
