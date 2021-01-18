/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "ui_PortfolioWidget.h"
#include "PortfolioWidget.h"
#include <QSortFilterProxyModel>
#include <QMenu>
#include <QClipboard>
#include <spdlog/spdlog.h>
#include "ApplicationSettings.h"
#include "CreateTransactionDialogAdvanced.h"
#include "BSMessageBox.h"
#include "TransactionDetailDialog.h"
#include "TransactionsViewModel.h"
#include "Wallets/SyncWalletsManager.h"
#include "Wallets/SyncWallet.h"
#include "UtxoReservationManager.h"


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
   : TransactionsWidgetInterface(parent)
   , ui_(new Ui::PortfolioWidget())
   , filter_(nullptr)
{
   ui_->setupUi(this);

   ui_->treeViewUnconfirmedTransactions->setContextMenuPolicy(Qt::CustomContextMenu);
   connect(ui_->treeViewUnconfirmedTransactions, &QTreeView::customContextMenuRequested, this, &PortfolioWidget::showContextMenu);
   connect(ui_->treeViewUnconfirmedTransactions, &QTreeView::activated, this, &PortfolioWidget::showTransactionDetails);
   ui_->treeViewUnconfirmedTransactions->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
}

PortfolioWidget::~PortfolioWidget() = default;

void PortfolioWidget::SetTransactionsModel(const std::shared_ptr<TransactionsViewModel>& model)
{
   model_ = model;
   filter_ = new UnconfirmedTransactionFilter(this);
   filter_->setSourceModel(model_.get());
   filter_->setDynamicSortFilter(true);

   ui_->treeViewUnconfirmedTransactions->setModel(filter_);
   ui_->treeViewUnconfirmedTransactions->setSortingEnabled(true);
   ui_->treeViewUnconfirmedTransactions->sortByColumn(static_cast<int>(TransactionsViewModel::Columns::Date)
      , Qt::SortOrder::DescendingOrder);
   ui_->treeViewUnconfirmedTransactions->hideColumn(
      static_cast<int>(TransactionsViewModel::Columns::TxHash));
}

void PortfolioWidget::init(const std::shared_ptr<ApplicationSettings> &appSettings
   , const std::shared_ptr<MarketDataProvider> &mdProvider
   , const std::shared_ptr<MDCallbacksQt> &mdCallbacks
   , const std::shared_ptr<CCPortfolioModel> &model
   , const std::shared_ptr<WalletSignerContainer> &container
   , const std::shared_ptr<ArmoryConnection> &armory
   , const std::shared_ptr<bs::UTXOReservationManager> &utxoReservationManager
   , const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<bs::sync::WalletsManager> &walletsMgr)
{
   //FIXME: TransactionsWidgetInterface::init(walletsMgr, armory, utxoReservationManager, container, appSettings, logger);

   ui_->widgetMarketData->init(appSettings, ApplicationSettings::Filter_MD_RFQ_Portfolio
      , mdProvider, mdCallbacks);
   ui_->widgetCCProtfolio->SetPortfolioModel(model);
}

void PortfolioWidget::shortcutActivated(ShortcutType s)
{

}

void PortfolioWidget::setAuthorized(bool authorized)
{
   ui_->widgetMarketData->setAuthorized(authorized);
}

void PortfolioWidget::onMDUpdated(bs::network::Asset::Type assetType
   , const QString& security, const bs::network::MDFields& fields)
{
   ui_->widgetMarketData->onMDUpdated(assetType, security, fields);
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

//FIXME:      TransactionDetailDialog transactionDetailDialog(txItem, walletsManager_, armory_, this);
//      transactionDetailDialog.exec();
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

   if (txNode->item()->isPayin()) {
      actionRevoke_->setData(sourceIndex);
//      actionRevoke_->setEnabled(model_->isTxRevocable(txNode->item()->tx));
      contextMenu_.addAction(actionRevoke_);
   }
   else {
      actionRevoke_->setData(-1);
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



#include "PortfolioWidget.moc"
