#include "MarketDataWidget.h"

#include "ui_MarketDataWidget.h"
#include "MarketDataProvider.h"
#include "MarketDataModel.h"


MarketDataWidget::MarketDataWidget(QWidget* parent)
   : QWidget(parent)
   , ui(new Ui::MarketDataWidget())
   , marketDataModel_(nullptr)
   , mdSortFilterModel_(nullptr)
{
   ui->setupUi(this);
}

MarketDataWidget::~MarketDataWidget()
{}

void MarketDataWidget::init(const std::shared_ptr<ApplicationSettings> &appSettings, ApplicationSettings::Setting param
   , const std::shared_ptr<MarketDataProvider>& mdProvider)
{
   QStringList visSettings;
   if (appSettings != nullptr) {
      settingVisibility_ = param;
      visSettings = appSettings->get<QStringList>(settingVisibility_);
      appSettings_ = appSettings;
   }
   marketDataModel_ = new MarketDataModel(visSettings, ui->treeViewMarketData);
   mdSortFilterModel_ = new MDSortFilterProxyModel(ui->treeViewMarketData);
   mdSortFilterModel_->setSourceModel(marketDataModel_);

   ui->treeViewMarketData->setModel(mdSortFilterModel_);
   ui->treeViewMarketData->setSortingEnabled(true);
   ui->treeViewMarketData->sortByColumn(0, Qt::AscendingOrder);

   if (appSettings != nullptr) {
      mdHeader_ = std::make_shared<MDHeader>(Qt::Horizontal, ui->treeViewMarketData);
      connect(mdHeader_.get(), &MDHeader::stateChanged, marketDataModel_, &MarketDataModel::onVisibilityToggled);
      connect(mdHeader_.get(), &MDHeader::stateChanged, this, &MarketDataWidget::onHeaderStateChanged);
      mdHeader_->setEnabled(false);
      mdHeader_->setToolTip(tr("Toggles filtered/selection view"));
      mdHeader_->setStretchLastSection(true);
      mdHeader_->show();
   }

   ui->treeViewMarketData->setHeader(mdHeader_.get());

   connect(marketDataModel_, &QAbstractItemModel::rowsInserted, [this]() {
      if (mdHeader_ != nullptr) {
         mdHeader_->setEnabled(true);
      }
   });
   connect(mdSortFilterModel_, &QAbstractItemModel::rowsInserted, this, &MarketDataWidget::resizeAndExpand);
   connect(marketDataModel_, &MarketDataModel::needResize, this, &MarketDataWidget::resizeAndExpand);

   connect(ui->treeViewMarketData, &QTreeView::clicked, this, &MarketDataWidget::onRowClicked);

   connect(mdProvider.get(), &MarketDataProvider::MDUpdate, marketDataModel_, &MarketDataModel::onMDUpdated);
   connect(mdProvider.get(), &MarketDataProvider::MDReqRejected, this, &MarketDataWidget::onMDRejected);
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
   if (!filteredView_) {
      return;
   }

   if (!index.parent().isValid()) {
      return;
   }

   auto pairIndex = mdSortFilterModel_->index(index.row(), static_cast<int>(MarketDataModel::MarketDataColumns::Product), index.parent());
   auto bidIndex = mdSortFilterModel_->index(index.row(), static_cast<int>(MarketDataModel::MarketDataColumns::BidPrice), index.parent());
   auto offerIndex = mdSortFilterModel_->index(index.row(), static_cast<int>(MarketDataModel::MarketDataColumns::OfferPrice), index.parent());

   QString group;
   QString currencyPair;

   group = mdSortFilterModel_->data(index.parent()).toString();
   currencyPair = mdSortFilterModel_->data(pairIndex).toString();

   QString bidPrice = mdSortFilterModel_->data(bidIndex).toString();
   QString offerPrice = mdSortFilterModel_->data(offerIndex).toString();

   switch (static_cast<MarketDataModel::MarketDataColumns>(index.column()))
   {
      case MarketDataModel::MarketDataColumns::BidPrice:
         emit BuyClicked(group, currencyPair, bidPrice, offerPrice);
         break;
      case MarketDataModel::MarketDataColumns::OfferPrice:
         emit SellClicked(group, currencyPair, bidPrice, offerPrice);
         break;
      default:
         emit CurrencySelected(group, currencyPair, bidPrice, offerPrice);
         break;
   }
}

void MarketDataWidget::resizeAndExpand()
{
   ui->treeViewMarketData->expandAll();
   ui->treeViewMarketData->resizeColumnToContents(0);
}

void MarketDataWidget::onHeaderStateChanged(bool state)
{
   filteredView_ = state;
   marketDataModel_->setHeaderData(0, Qt::Horizontal, state ? tr("Filtered view") : tr("Visibility selection"));

   if (state && (appSettings_ != nullptr)) {
      const auto settings = marketDataModel_->getVisibilitySettings();
      appSettings_->set(settingVisibility_, settings);
   }
}
