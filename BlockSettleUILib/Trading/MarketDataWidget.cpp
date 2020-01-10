/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "MarketDataWidget.h"

#include "ui_MarketDataWidget.h"
#include "MarketDataProvider.h"
#include "MarketDataModel.h"
#include "TreeViewWithEnterKey.h"

constexpr int EMPTY_COLUMN_WIDTH = 0;

bool MarketSelectedInfo::isValid() const
{
   return !productGroup_.isEmpty() &&
      !currencyPair_.isEmpty() &&
      !bidPrice_.isEmpty() &&
      !offerPrice_.isEmpty()
      ;
}

MarketDataWidget::MarketDataWidget(QWidget* parent)
   : QWidget(parent)
   , ui_(new Ui::MarketDataWidget())
   , marketDataModel_(nullptr)
   , mdSortFilterModel_(nullptr)
{
   ui_->setupUi(this);
}

MarketDataWidget::~MarketDataWidget()
{}

void MarketDataWidget::init(const std::shared_ptr<ApplicationSettings> &appSettings, ApplicationSettings::Setting param
   , const std::shared_ptr<MarketDataProvider>& mdProvider)
{
   mdProvider_ = mdProvider;

   QStringList visSettings;
   if (appSettings != nullptr) {
      settingVisibility_ = param;
      visSettings = appSettings->get<QStringList>(settingVisibility_);
      appSettings_ = appSettings;
   }
   marketDataModel_ = new MarketDataModel(visSettings, ui_->treeViewMarketData);
   mdSortFilterModel_ = new MDSortFilterProxyModel(ui_->treeViewMarketData);
   mdSortFilterModel_->setSourceModel(marketDataModel_);

   ui_->treeViewMarketData->setModel(mdSortFilterModel_);
   ui_->treeViewMarketData->setSortingEnabled(true);

   if (appSettings != nullptr) {
      mdHeader_ = std::make_shared<MDHeader>(Qt::Horizontal, ui_->treeViewMarketData);
      connect(mdHeader_.get(), &MDHeader::stateChanged, marketDataModel_, &MarketDataModel::onVisibilityToggled);
      connect(mdHeader_.get(), &MDHeader::stateChanged, this, &MarketDataWidget::onHeaderStateChanged);
      mdHeader_->setEnabled(false);
      mdHeader_->setToolTip(tr("Toggles filtered/selection view"));
      mdHeader_->setStretchLastSection(true);
      mdHeader_->show();
   }

   ui_->treeViewMarketData->setHeader(mdHeader_.get());
   ui_->treeViewMarketData->header()->setSortIndicator(
      static_cast<int>(MarketDataModel::MarketDataColumns::First), Qt::AscendingOrder);
   ui_->treeViewMarketData->header()->resizeSection(static_cast<int>(MarketDataModel::MarketDataColumns::EmptyColumn),
                                                    EMPTY_COLUMN_WIDTH);

   connect(marketDataModel_, &QAbstractItemModel::rowsInserted, [this]() {
      if (mdHeader_ != nullptr) {
         mdHeader_->setEnabled(true);
      }
   });
   connect(mdSortFilterModel_, &QAbstractItemModel::rowsInserted, this, &MarketDataWidget::resizeAndExpand);
   connect(marketDataModel_, &MarketDataModel::needResize, this, &MarketDataWidget::resizeAndExpand);

   connect(ui_->treeViewMarketData, &QTreeView::clicked, this, &MarketDataWidget::onRowClicked);
   connect(ui_->treeViewMarketData, &TreeViewWithEnterKey::enterKeyPressed,
           this, &MarketDataWidget::onEnterKeyPressed);

   connect(mdProvider.get(), &MarketDataProvider::MDUpdate, marketDataModel_, &MarketDataModel::onMDUpdated);
   connect(mdProvider.get(), &MarketDataProvider::MDReqRejected, this, &MarketDataWidget::onMDRejected);

   connect(ui_->pushButtonMDConnection, &QPushButton::clicked, this, &MarketDataWidget::ChangeMDSubscriptionState);

   connect(mdProvider.get(), &MarketDataProvider::WaitingForConnectionDetails, this, &MarketDataWidget::onLoadingNetworkSettings);
   connect(mdProvider.get(), &MarketDataProvider::StartConnecting, this, &MarketDataWidget::OnMDConnecting);
   connect(mdProvider.get(), &MarketDataProvider::Connected, this, &MarketDataWidget::OnMDConnected);
   connect(mdProvider.get(), &MarketDataProvider::Disconnecting, this, &MarketDataWidget::OnMDDisconnecting);
   connect(mdProvider.get(), &MarketDataProvider::Disconnected, this, &MarketDataWidget::OnMDDisconnected);

   ui_->pushButtonMDConnection->setText(tr("Subscribe"));
}

void MarketDataWidget::onLoadingNetworkSettings()
{
   ui_->pushButtonMDConnection->setText(tr("Connecting"));
   ui_->pushButtonMDConnection->setEnabled(false);
   ui_->pushButtonMDConnection->setToolTip(tr("Waiting for connection details"));
}

void MarketDataWidget::OnMDConnecting()
{
   ui_->pushButtonMDConnection->setText(tr("Connecting"));
   ui_->pushButtonMDConnection->setEnabled(false);
   ui_->pushButtonMDConnection->setToolTip(QString{});
}

void MarketDataWidget::OnMDConnected()
{
   ui_->pushButtonMDConnection->setText(tr("Disconnect"));
   ui_->pushButtonMDConnection->setEnabled(!authorized_);
}

void MarketDataWidget::OnMDDisconnecting()
{
   ui_->pushButtonMDConnection->setText(tr("Disconnecting"));
   ui_->pushButtonMDConnection->setEnabled(false);
   mdProvider_->UnsubscribeFromMD();
}

void MarketDataWidget::OnMDDisconnected()
{
   ui_->pushButtonMDConnection->setText(tr("Subscribe"));
   ui_->pushButtonMDConnection->setEnabled(!authorized_);
}

void MarketDataWidget::ChangeMDSubscriptionState()
{
   if (mdProvider_->IsConnectionActive()) {
      mdProvider_->DisconnectFromMDSource();
   } else {
      mdProvider_->SubscribeToMD();
   }
}

MarketSelectedInfo MarketDataWidget::getRowInfo(const QModelIndex& index) const
{
   if (!index.isValid() || !index.parent().isValid()) {
      return {};
   }

   auto pairIndex = mdSortFilterModel_->index(index.row(), static_cast<int>(MarketDataModel::MarketDataColumns::Product), index.parent());
   auto bidIndex = mdSortFilterModel_->index(index.row(), static_cast<int>(MarketDataModel::MarketDataColumns::BidPrice), index.parent());
   auto offerIndex = mdSortFilterModel_->index(index.row(), static_cast<int>(MarketDataModel::MarketDataColumns::OfferPrice), index.parent());

   MarketSelectedInfo selectedInfo;
   selectedInfo.productGroup_ = mdSortFilterModel_->data(index.parent()).toString();
   selectedInfo.currencyPair_ = mdSortFilterModel_->data(pairIndex).toString();
   selectedInfo.bidPrice_ = mdSortFilterModel_->data(bidIndex).toString();
   selectedInfo.offerPrice_ = mdSortFilterModel_->data(offerIndex).toString();

   return selectedInfo;
}

TreeViewWithEnterKey* MarketDataWidget::view() const
{
   return ui_->treeViewMarketData;
}

void MarketDataWidget::setAuthorized(bool authorized)
{
   ui_->pushButtonMDConnection->setEnabled(!authorized);
   authorized_ = authorized;
}

MarketSelectedInfo MarketDataWidget::getCurrentlySelectedInfo() const
{
   if (!ui_->treeViewMarketData) {
      return {};
   }

   const QModelIndex index = ui_->treeViewMarketData->selectionModel()->currentIndex();
   return getRowInfo(index);
}

void MarketDataWidget::onMDRejected(const std::string &security, const std::string &reason)
{
   if (security.empty()) {
      return;
   }
   bs::network::MDFields mdFields = { { bs::network::MDField::Reject, 0, QString::fromStdString(reason) } };
   marketDataModel_->onMDUpdated(bs::network::Asset::Undefined, QString::fromStdString(security), mdFields);
}

void MarketDataWidget::onRowClicked(const QModelIndex& index)
{
   if (!filteredView_ || !index.isValid()) {
      return;
   }

   // Tab clicked
   if (!index.parent().isValid()) {
      emit MDHeaderClicked();
      return;
   }

   MarketSelectedInfo selectedInfo = getRowInfo(index);

   switch (static_cast<MarketDataModel::MarketDataColumns>(index.column()))
   {
   case MarketDataModel::MarketDataColumns::BidPrice: {
      emit BidClicked(selectedInfo);
      break;
   }
   case MarketDataModel::MarketDataColumns::OfferPrice: {
      emit AskClicked(selectedInfo);
      break;
   }
   default: {
      emit CurrencySelected(selectedInfo);
      break;
   }
   }
}

void MarketDataWidget::onEnterKeyPressed(const QModelIndex &index)
{
   auto pairIndex = mdSortFilterModel_->index(index.row(),
      static_cast<int>(MarketDataModel::MarketDataColumns::Product), index.parent());

   onRowClicked(pairIndex);
}

void MarketDataWidget::resizeAndExpand()
{
   ui_->treeViewMarketData->expandAll();
   ui_->treeViewMarketData->resizeColumnToContents(0);
   ui_->treeViewMarketData->header()->resizeSection(static_cast<int>(MarketDataModel::MarketDataColumns::EmptyColumn),
                                                    EMPTY_COLUMN_WIDTH);
}

void MarketDataWidget::onHeaderStateChanged(bool state)
{
   filteredView_ = state;
   marketDataModel_->setHeaderData(0, Qt::Horizontal, state ? tr("Filtered view") : tr("Visibility selection"));
   ui_->treeViewMarketData->resizeColumnToContents(0);
   ui_->treeViewMarketData->header()->resizeSection(static_cast<int>(MarketDataModel::MarketDataColumns::EmptyColumn),
                                                    EMPTY_COLUMN_WIDTH);

   if (state && (appSettings_ != nullptr)) {
      const auto settings = marketDataModel_->getVisibilitySettings();
      appSettings_->set(settingVisibility_, settings);
   }
}
