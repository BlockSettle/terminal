#include "ui_PortfolioWidget.h"
#include "PortfolioWidget.h"
#include <QSortFilterProxyModel>
#include <QMenu>
#include <QClipboard>

#include "ApplicationSettings.h"
#include "CreateTransactionDialogAdvanced.h"
#include "BSMessageBox.h"
#include "TransactionDetailDialog.h"
#include "TransactionsViewModel.h"
#include "Wallets/SyncWalletsManager.h"
#include "Wallets/SyncWallet.h"


class UnconfirmedTransactionFilter : public QSortFilterProxyModel
{
   Q_OBJECT

public:
   UnconfirmedTransactionFilter(QObject* parent) : QSortFilterProxyModel(parent) {
      setSortRole(TransactionsViewModel::SortRole);
   }
   virtual ~UnconfirmedTransactionFilter() = default;

   QVariant data(const QModelIndex& proxyIndex, int role = Qt::DisplayRole) const override {
      const auto &result = QSortFilterProxyModel::data(proxyIndex, role);
      if ((role == Qt::DisplayRole) && (proxyIndex.column() == static_cast<int>(TransactionsViewModel::Columns::Status))) {
         return tr("%1/%2").arg(result.toString()).arg(confThreshold_);
      }
      return result;
   }

protected:
   bool filterAcceptsRow(int source_row, const QModelIndex & source_parent) const override {
      if (!sourceModel() || (sourceModel()->rowCount() == 0)) {
         return false;
      }
      QModelIndex index = sourceModel()->index(source_row, static_cast<int>(TransactionsViewModel::Columns::Status), source_parent);
      int confirmations = sourceModel()->data(index).toInt();
      return confirmations < confThreshold_;
   }

private:
   const int   confThreshold_ = 6;
};


PortfolioWidget::PortfolioWidget(QWidget* parent)
   : TabWithShortcut(parent)
   , ui_(new Ui::PortfolioWidget())
   , model_(nullptr)
   , filter_(nullptr)
{
   ui_->setupUi(this);

   ui_->treeViewUnconfirmedTransactions->setContextMenuPolicy(Qt::CustomContextMenu);
   connect(ui_->treeViewUnconfirmedTransactions, &QTreeView::customContextMenuRequested, this, &PortfolioWidget::showContextMenu);
   connect(ui_->treeViewUnconfirmedTransactions, &QTreeView::activated, this, &PortfolioWidget::showTransactionDetails);
   ui_->treeViewUnconfirmedTransactions->header()->setSectionResizeMode(QHeaderView::ResizeToContents);

   actionCopyAddr_ = new QAction(tr("&Copy Address"));
   connect(actionCopyAddr_, &QAction::triggered, [this]() {
      qApp->clipboard()->setText(curAddress_);
   });

   actionCopyTx_ = new QAction(tr("Copy &Transaction Hash"));
   connect(actionCopyTx_, &QAction::triggered, [this]() {
      qApp->clipboard()->setText(curTx_);
   });

   actionRBF_ = new QAction(tr("Replace-By-Fee (RBF)"), this);
   connect(actionRBF_, &QAction::triggered, this, &PortfolioWidget::onCreateRBFDialog);

   actionCPFP_ = new QAction(tr("Child-Pays-For-Parent (CPFP)"), this);
   connect(actionCPFP_, &QAction::triggered, this, &PortfolioWidget::onCreateCPFPDialog);
}

PortfolioWidget::~PortfolioWidget() = default;

void PortfolioWidget::SetTransactionsModel(const std::shared_ptr<TransactionsViewModel>& model)
{
   model_ = model;
   filter_ = new UnconfirmedTransactionFilter(this);
   filter_->setSourceModel(model_.get());
   ui_->treeViewUnconfirmedTransactions->setModel(filter_);
   ui_->treeViewUnconfirmedTransactions->setSortingEnabled(true);
   ui_->treeViewUnconfirmedTransactions->sortByColumn(static_cast<int>(TransactionsViewModel::Columns::Date)
      , Qt::SortOrder::DescendingOrder);
   ui_->treeViewUnconfirmedTransactions->hideColumn(
      static_cast<int>(TransactionsViewModel::Columns::TxHash));
}

void PortfolioWidget::init(const std::shared_ptr<ApplicationSettings> &appSettings
   , const std::shared_ptr<MarketDataProvider> &mdProvider
   , const std::shared_ptr<CCPortfolioModel> &model
   , const std::shared_ptr<SignContainer> &container
   , const std::shared_ptr<ArmoryConnection> &armory
   , const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<bs::sync::WalletsManager> &walletsMgr)
{
   signContainer_ = container;
   armory_ = armory;
   walletsManager_ = walletsMgr;
   logger_ = logger;
   appSettings_ = appSettings;

   ui_->widgetMarketData->init(appSettings, ApplicationSettings::Filter_MD_RFQ_Portfolio, mdProvider);
   ui_->widgetCCProtfolio->SetPortfolioModel(model);
}

void PortfolioWidget::shortcutActivated(ShortcutType s)
{

}

void PortfolioWidget::setAuthorized(bool authorized)
{
   ui_->widgetMarketData->setAuthorized(authorized);
}

void PortfolioWidget::showTransactionDetails(const QModelIndex& index)
{
   if (filter_) {
      QModelIndex sourceIndex = filter_->mapToSource(index);
      const auto &txItem = model_->getItem(sourceIndex);
      if (!txItem) {
         SPDLOG_LOGGER_ERROR(logger_, "item not found");
         return;
      }

      TransactionDetailDialog transactionDetailDialog(txItem, walletsManager_, armory_, this);
      transactionDetailDialog.exec();
   }
}

void PortfolioWidget::showContextMenu(const QPoint &point)
{
   if (!filter_) {
      return;
   }

   const auto sourceIndex = filter_->mapToSource(ui_->treeViewUnconfirmedTransactions->indexAt(point));
   auto addressIndex = model_->index(sourceIndex.row(), static_cast<int>(TransactionsViewModel::Columns::Address));
   curAddress_ = model_->data(addressIndex).toString();

   const auto txNode = model_->getNode(sourceIndex);
   contextMenu_.clear();
   if (!txNode || !txNode->item() || !txNode->item()->initialized) {
      return;
   }

   if (txNode->item()->isRBFeligible() && (txNode->level() < 2)) {
      contextMenu_.addAction(actionRBF_);
      actionRBF_->setData(sourceIndex);
   } else {
      actionRBF_->setData(-1);
   }

   if (txNode->item()->isCPFPeligible()) {
      contextMenu_.addAction(actionCPFP_);
      actionCPFP_->setData(sourceIndex);
   } else {
      actionCPFP_->setData(-1);
   }

   // save transaction id and add context menu for copying it to clipboard
   curTx_ = QString::fromStdString(txNode->item()->txEntry.txHash.toHexStr(true));
   contextMenu_.addAction(actionCopyTx_);

   // allow copy address only if there is only 1 address
   if (txNode->item()->addressCount == 1) {
      contextMenu_.addAction(actionCopyAddr_);
   }

   if (!contextMenu_.isEmpty()) {
      contextMenu_.exec(ui_->treeViewUnconfirmedTransactions->mapToGlobal(point));
   }
}

void PortfolioWidget::onCreateRBFDialog()
{
   const auto &txItem = model_->getItem(actionRBF_->data().toModelIndex());
   if (!txItem) {
      SPDLOG_LOGGER_ERROR(logger_, "item not found");
      return;
   }

   const auto &cbDialog = [this](const TransactionPtr &txItem) {
      try {
         auto dlg = CreateTransactionDialogAdvanced::CreateForRBF(armory_
            , walletsManager_, signContainer_, logger_, appSettings_, txItem->tx
            , this);
         dlg->exec();
      }
      catch (const std::exception &e) {
         BSMessageBox(BSMessageBox::critical, tr("RBF Transaction"), tr("Failed to create RBF transaction")
            , QLatin1String(e.what()), this).exec();
      }
   };

   if (txItem->initialized) {
      cbDialog(txItem);
   }
   else {
      TransactionsViewItem::initialize(txItem, armory_.get(), walletsManager_, cbDialog);
   }
}

void PortfolioWidget::onCreateCPFPDialog()
{
   const auto &txItem = model_->getItem(actionCPFP_->data().toModelIndex());
   if (!txItem) {
      SPDLOG_LOGGER_ERROR(logger_, "item not found");
      return;
   }

   const auto &cbDialog = [this](const TransactionPtr &txItem) {
      try {
         auto dlg = CreateTransactionDialogAdvanced::CreateForCPFP(armory_
            , walletsManager_, signContainer_, txItem->wallet, logger_, appSettings_
            , txItem->tx, this);
         dlg->exec();
      }
      catch (const std::exception &e) {
         BSMessageBox(BSMessageBox::critical, tr("CPFP Transaction"), tr("Failed to create CPFP transaction")
            , QLatin1String(e.what()), this).exec();
      }
   };

   if (txItem->initialized) {
      cbDialog(txItem);
   }
   else {
      TransactionsViewItem::initialize(txItem, armory_.get(), walletsManager_, cbDialog);
   }
}


#include "PortfolioWidget.moc"
