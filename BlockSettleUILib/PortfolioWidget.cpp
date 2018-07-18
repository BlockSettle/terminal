#include "PortfolioWidget.h"

#include "ui_PortfolioWidget.h"

#include "ApplicationSettings.h"
#include "CreateTransactionDialogAdvanced.h"
#include "MessageBoxCritical.h"
#include "MetaData.h"
#include "TransactionDetailDialog.h"
#include "TransactionsViewModel.h"
#include "WalletsManager.h"

#include <QSortFilterProxyModel>
#include <QMenu>


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

   actionRBF_ = new QAction(tr("Replace-By-Fee (RBF)"), this);
   connect(actionRBF_, &QAction::triggered, this, &PortfolioWidget::onCreateRBFDialog);

   actionCPFP_ = new QAction(tr("Child-Pays-For-Parent (CPFP)"), this);
   connect(actionCPFP_, &QAction::triggered, this, &PortfolioWidget::onCreateCPFPDialog);
}

void PortfolioWidget::SetTransactionsModel(const std::shared_ptr<TransactionsViewModel>& model)
{
   model_ = model;
   filter_ = new UnconfirmedTransactionFilter(this);
   filter_->setSourceModel(model_.get());
   ui_->treeViewUnconfirmedTransactions->setModel(filter_);
   ui_->treeViewUnconfirmedTransactions->setSortingEnabled(true);
   ui_->treeViewUnconfirmedTransactions->sortByColumn(static_cast<int>(TransactionsViewModel::Columns::Date)
      , Qt::SortOrder::DescendingOrder);
}

void PortfolioWidget::init(const std::shared_ptr<ApplicationSettings> &appSettings
   , const std::shared_ptr<MarketDataProvider> &mdProvider, const std::shared_ptr<CCPortfolioModel> &model
   , const std::shared_ptr<SignContainer> &container, const std::shared_ptr<ArmoryConnection> &armory
   , const std::shared_ptr<WalletsManager> &walletsMgr)
{
   signContainer_ = container;
   armory_ = armory;
   walletsManager_ = walletsMgr;

   ui_->widgetMarketData->init(appSettings, ApplicationSettings::Filter_MD_RFQ_Portfolio, mdProvider);
   ui_->widgetCCProtfolio->SetPortfolioModel(model);
}

void PortfolioWidget::shortcutActivated(ShortcutType s)
{

}

void PortfolioWidget::showTransactionDetails(const QModelIndex& index)
{
   if (filter_) {
      QModelIndex sourceIndex = filter_->mapToSource(index);
      auto txItem = model_->getItem(sourceIndex.row());

      TransactionDetailDialog transactionDetailDialog(txItem, walletsManager_, armory_, this);
      transactionDetailDialog.exec();
   }
}

static bool isRBFEligible(const TransactionsViewItem &item)
{
   return ((item.confirmations == 0)
      && item.led->isOptInRBF()
      && ( item.wallet != nullptr && item.wallet->GetType() != bs::wallet::Type::Settlement)
      && ( item.direction == bs::Transaction::Direction::Internal || item.direction == bs::Transaction::Direction::Sent ));
}

static bool isCPFPEligible(const TransactionsViewItem &item)
{
   return ((item.confirmations == 0) && (item.wallet != nullptr && item.wallet->GetType() != bs::wallet::Type::Settlement)
         && (item.direction == bs::Transaction::Direction::Internal || item.direction == bs::Transaction::Direction::Received));
}

void PortfolioWidget::showContextMenu(const QPoint &point)
{
   if (!filter_) {
      return;
   }

   const auto sourceIndex = filter_->mapToSource(ui_->treeViewUnconfirmedTransactions->indexAt(point));
   const auto txItem = model_->getItem(sourceIndex.row());
   contextMenu_.clear();
   if (!txItem.initialized) {
      return;
   }

   if (isRBFEligible(txItem)) {
      contextMenu_.addAction(actionRBF_);
      actionRBF_->setData(sourceIndex.row());
   } else {
      actionRBF_->setData(-1);
   }

   if (isCPFPEligible(txItem)) {
      contextMenu_.addAction(actionCPFP_);
      actionCPFP_->setData(sourceIndex.row());
   } else {
      actionCPFP_->setData(-1);
   }

   contextMenu_.exec(ui_->treeViewUnconfirmedTransactions->mapToGlobal(point));
}

void PortfolioWidget::onCreateRBFDialog()
{
   auto txItem = model_->getItem(actionRBF_->data().toInt());

   const auto &cbDialog = [this, txItem] {
      try {
         auto dlg = CreateTransactionDialogAdvanced::CreateForRBF(armory_
            , walletsManager_, signContainer_
            , txItem.tx, txItem.wallet
            , this);
         dlg->exec();
      }
      catch (const std::exception &e) {
         MessageBoxCritical(tr("RBF Transaction"), tr("Failed to create RBF transaction")
            , QLatin1String(e.what()), this).exec();
      }
   };

   if (txItem.initialized) {
      cbDialog();
   }
   else {
      txItem.initialize(armory_, walletsManager_, cbDialog);
   }
}

void PortfolioWidget::onCreateCPFPDialog()
{
   auto txItem = model_->getItem(actionCPFP_->data().toInt());

   const auto &cbDialog = [this, txItem] {
      try {
         auto dlg = CreateTransactionDialogAdvanced::CreateForCPFP(armory_
            , walletsManager_, signContainer_
            , txItem.wallet, txItem.tx
            , this);
         dlg->exec();
      }
      catch (const std::exception &e) {
         MessageBoxCritical(tr("CPFP Transaction"), tr("Failed to create CPFP transaction")
            , QLatin1String(e.what()), this).exec();
      }
   };

   if (txItem.initialized) {
      cbDialog();
   }
   else {
      txItem.initialize(armory_, walletsManager_, cbDialog);
   }
}


#include "PortfolioWidget.moc"
