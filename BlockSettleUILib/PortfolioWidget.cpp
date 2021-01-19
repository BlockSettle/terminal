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
#include "BSMessageBox.h"
#include "CCPortfolioModel.h"
#include "CreateTransactionDialogAdvanced.h"
#include "CurrencyPair.h"
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

void PortfolioWidget::init(const std::shared_ptr<spdlog::logger>& logger)
{
   logger_ = logger;
   portfolioModel_ = std::make_shared<CCPortfolioModel>(this);
   ui_->widgetCCProtfolio->SetPortfolioModel(portfolioModel_);
}

void PortfolioWidget::shortcutActivated(ShortcutType s)
{}

void PortfolioWidget::setAuthorized(bool authorized)
{
   ui_->widgetMarketData->setAuthorized(authorized);
}

void PortfolioWidget::onMDUpdated(bs::network::Asset::Type assetType
   , const QString& security, const bs::network::MDFields& fields)
{
   ui_->widgetMarketData->onMDUpdated(assetType, security, fields);

   if ((assetType == bs::network::Asset::Undefined) || security.isEmpty()) {
      return;
   }
   double lastPx = 0;
   double bidPrice = 0;

   double productPrice = 0;
   CurrencyPair cp(security.toStdString());
   std::string ccy;

   switch (assetType) {
   case bs::network::Asset::PrivateMarket:
      ccy = cp.NumCurrency();
      break;
   case bs::network::Asset::SpotXBT:
      ccy = cp.DenomCurrency();
      break;
   default:
      return;
   }

   if (ccy.empty()) {
      return;
   }

   for (const auto& field : fields) {
      if (field.type == bs::network::MDField::PriceLast) {
         lastPx = field.value;
         break;
      } else  if (field.type == bs::network::MDField::PriceBid) {
         bidPrice = field.value;
      }
   }

   productPrice = (lastPx > 0) ? lastPx : bidPrice;

   if (productPrice > 0) {
      if (ccy == cp.DenomCurrency()) {
         productPrice = 1 / productPrice;
      }
      portfolioModel_->onPriceChanged(ccy, productPrice);
      ui_->widgetCCProtfolio->onPriceChanged(ccy, productPrice);
      if (ccy == "EUR") {
         ui_->widgetCCProtfolio->onBasePriceChanged(ccy, 1/productPrice);
      }
   }
}

void PortfolioWidget::onHDWallet(const bs::sync::WalletInfo& wi)
{
   portfolioModel_->onHDWallet(wi);
}

void PortfolioWidget::onHDWalletDetails(const bs::sync::HDWalletData& wd)
{
   portfolioModel_->onHDWalletDetails(wd);
}

void PortfolioWidget::onWalletBalance(const bs::sync::WalletBalanceData& wbd)
{
   portfolioModel_->onWalletBalance(wbd);
   ui_->widgetCCProtfolio->onWalletBalance(wbd);
}

void PortfolioWidget::onBalance(const std::string& currency, double balance)
{
   portfolioModel_->onBalance(currency, balance);
   ui_->widgetCCProtfolio->onBalance(currency, balance);
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

      auto txDetailDialog = new TransactionDetailDialog(txItem, this);
      connect(txDetailDialog, &QDialog::finished, [txDetailDialog](int) {
         txDetailDialog->deleteLater();
      });
      txDetailDialog->show();
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
