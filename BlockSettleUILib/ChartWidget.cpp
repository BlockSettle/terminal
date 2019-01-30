#include "ChartWidget.h"
#include "ui_ChartWidget.h"
#include "ApplicationSettings.h"
#include "MarketDataProvider.h"
#include "ArmoryConnection.h"
#include <qrandom.h>
#include "CustomCandlestickSet.h"
#include "TradesClient.h"


ChartWidget::ChartWidget(QWidget *parent)
   : QWidget(parent)
   , ui_(new Ui::ChartWidget) {
   ui_->setupUi(this);

   // setting up date range radio button group
   dateRange_.addButton(ui_->btn1h, TradesDB::Interval::OneHour);
   dateRange_.addButton(ui_->btn6h, TradesDB::Interval::SixHours);
   dateRange_.addButton(ui_->btn12h, TradesDB::Interval::TwelveHours);
   dateRange_.addButton(ui_->btn24h, TradesDB::Interval::TwentyFourHours);
   dateRange_.addButton(ui_->btn1w, TradesDB::Interval::OneWeek);
   dateRange_.addButton(ui_->btn1m, TradesDB::Interval::OneMonth);
   dateRange_.addButton(ui_->btn6m, TradesDB::Interval::SixMonths);
   dateRange_.addButton(ui_->btn1y, TradesDB::Interval::OneYear);
   connect(&dateRange_, qOverload<int>(&QButtonGroup::buttonClicked), 
      this, &ChartWidget::onDateRangeChanged);

   // these signals are used to sync volume chart with price chart
   // when scrolling and zooming
   connect(ui_->viewPrice, &CustomChartView::chartZoomed,
      ui_->viewVolume, &CustomChartView::onChartZoomed);
   connect(ui_->viewPrice, &CustomChartView::chartScrolled,
      ui_->viewVolume, &CustomChartView::onChartScrolled);
   connect(ui_->viewPrice, &CustomChartView::chartZoomReset,
      ui_->viewVolume, &CustomChartView::onChartZoomReset);
   // drag and zoom is enabled only for the price chart
   ui_->viewPrice->enableDrag(true);
   ui_->viewPrice->enableZoom(true);

   connect(ui_->cboInstruments, &QComboBox::currentTextChanged,
      this, &ChartWidget::onInstrumentChanged);

   // sort model for instruments combo box
   cboModel_ = new QStandardItemModel(this);
   auto proxy = new QSortFilterProxyModel();
   proxy->setSourceModel(cboModel_);
   proxy->sort(0);
   ui_->cboInstruments->setModel(proxy);

   priceSeries_ = new QCandlestickSeries(this);
   priceSeries_->setIncreasingColor(QColor(34, 192, 100));
   priceSeries_->setDecreasingColor(QColor(207, 41, 46));
   QPen pen(QRgb(0xffffff));
   pen.setWidth(1);
   priceSeries_->setPen(pen);
   priceSeries_->setBodyOutlineVisible(false);
   connect(priceSeries_, &QCandlestickSeries::hovered, this, &ChartWidget::onPriceHover);

   volumeSeries_ = new QCandlestickSeries(this);
   volumeSeries_->setIncreasingColor(QColor(32, 159, 223));
   volumeSeries_->setBodyOutlineVisible(false);

   dataItemText_ = new QGraphicsTextItem(ui_->viewPrice->chart());
}

void ChartWidget::init(const std::shared_ptr<ApplicationSettings> &appSettings
                       , const std::shared_ptr<MarketDataProvider> &mdProvider
                       , const std::shared_ptr<ArmoryConnection> &
                       , const std::shared_ptr<spdlog::logger>& logger) {
   mdProvider_ = mdProvider;
   client_ = std::make_shared<TradesClient>(appSettings, logger);
   client_->init();

   connect(mdProvider.get(), &MarketDataProvider::MDUpdate, this, &ChartWidget::onMDUpdated);
   connect(mdProvider.get(), &MarketDataProvider::MDUpdate, client_.get(), &TradesClient::onMDUpdated);

   // populate charts with data
   buildCandleChart();
   // style the charts
   setChartStyle();
   // setup the chart axis
   createCandleChartAxis();
   createVolumeChartAxis();
}

ChartWidget::~ChartWidget() {
    delete ui_;
}

// Populate combo box with existing instruments comeing from mdProvider
void ChartWidget::onMDUpdated(bs::network::Asset::Type assetType, const QString &security, bs::network::MDFields mdFields) {
   auto cbo = ui_->cboInstruments;
   if ((assetType == bs::network::Asset::Undefined) && security.isEmpty()) {  // Celer disconnected
      cboModel_->clear();
      return;
   }
   
   if (cboModel_->findItems(security).isEmpty()) {
      cboModel_->appendRow(new QStandardItem(security));
   }
}

// Sets up the styling of both candlesticka
// and volume charts.
void ChartWidget::setChartStyle() {
   QBrush bgBrush(QColor(28, 40, 53));

   // candle stick chart
   auto priceChart = ui_->viewPrice->chart();
   priceChart->setBackgroundBrush(bgBrush);
   // Customize chart title
   QFont font;
   font.setPixelSize(18);
   priceChart->setTitleFont(font);
   priceChart->setTitleBrush(QBrush(Qt::white));
   priceChart->legend()->setVisible(false);
   qreal left, top, right, bottom;
   priceChart->layout()->getContentsMargins(&left, &top, &right, &bottom);
   bottom = 0.0;
   priceChart->layout()->setContentsMargins(left, top, right, bottom);
   auto mainMargins = priceChart->margins();
   mainMargins.setBottom(0);
   priceChart->setMargins(mainMargins);

   // this item is used to display price of individual candle sticks
   dataItemText_->setPos(20, 20);
   dataItemText_->setDefaultTextColor(QColor(Qt::white));

   // volume chart
   auto volChart = ui_->viewVolume->chart();
   volChart->setBackgroundBrush(bgBrush);
   // remove top and bottom margins for the volume chart
   auto margins = volChart->margins();
   margins.setTop(0);
   margins.setBottom(5);
   volChart->setMargins(margins);
   volChart->legend()->setVisible(false);
   volChart->layout()->getContentsMargins(&left, &top, &right, &bottom);
   top = 0.0;
   volChart->layout()->setContentsMargins(left, top, right, bottom);
}

// Creates x and y axis for the candle stick chart.
void ChartWidget::createCandleChartAxis() {
   auto dateAxisx = new QDateTimeAxis(this);
   auto chart = ui_->viewPrice->chart();
   //auto series = qobject_cast<QCandlestickSeries *>(chart->series().at(0));
   dateAxisx->setFormat(tr("d-M-yy"));
   //dateAxisx->setTitleText(tr("Date"));
   dateAxisx->setTitleBrush(QBrush(Qt::white));
   // hide grid lines
   dateAxisx->setGridLineVisible(false);
   dateAxisx->setMinorGridLineVisible(false);
   dateAxisx->setLabelsColor(QColor(Qt::white));
   chart->addAxis(dateAxisx, Qt::AlignBottom);
   chart->series().first()->attachAxis(dateAxisx);
   dateAxisx->setTickCount(10);
   dateAxisx->setLabelsVisible(false);
   // add space offset to the x axis
   // this code will eventually have to be moved to data range handler function
   // and will need to be called when date range is modified
   /*dateAxisx->setMax(dateAxisx->max().addDays(1));
   dateAxisx->setMin(dateAxisx->min().addDays(-1));*/

   // y axis
   QValueAxis *axisY = new QValueAxis;
   axisY->setLabelsColor(QColor(Qt::white));
   axisY->setGridLineVisible(false);
   axisY->setMinorGridLineVisible(false);
   chart->addAxis(axisY, Qt::AlignRight);
   axisY->setLabelFormat(tr("%06.1f"));
   qDebug() << "chart localize" << chart->localizeNumbers();
   chart->series().first()->attachAxis(axisY);
   // add space offset to the y axis
   auto tmpAxis = qobject_cast<QValueAxis *>(chart->axes(Qt::Vertical).at(0));
   tmpAxis->setMax(tmpAxis->max() * 1.05);
   tmpAxis->setMin(tmpAxis->min() * 0.95);
   tmpAxis->applyNiceNumbers();
}

// Creates x and y axis for the volume stick chart.
void ChartWidget::createVolumeChartAxis() {
   auto dateAxisx = new QDateTimeAxis(this);
   auto chart = ui_->viewVolume->chart();
   dateAxisx->setFormat(tr("d-M-yy"));
   // hide grid lines
   dateAxisx->setGridLineVisible(false);
   dateAxisx->setMinorGridLineVisible(false);
   dateAxisx->setLabelsColor(QColor(Qt::white));
   chart->addAxis(dateAxisx, Qt::AlignBottom);
   chart->series().first()->attachAxis(dateAxisx);
   dateAxisx->setTickCount(10);
   dateAxisx->setLabelsVisible(true);
   // add space offset to the x axis
   // this code will eventually have to be moved to data range handler function
   // and will need to be called when date range is modified
   /*dateAxisx->setMax(dateAxisx->max().addDays(1));
   dateAxisx->setMin(dateAxisx->min().addDays(-1));*/

   QValueAxis *axisY = new QValueAxis;
   axisY->setLabelFormat(tr("%06.1f"));
   axisY->setLabelsColor(QColor(Qt::white));
   axisY->setGridLineVisible(false);
   axisY->setMinorGridLineVisible(false);
   axisY->setTickCount(2);
   chart->addAxis(axisY, Qt::AlignRight);
   chart->series().first()->attachAxis(axisY);
   axisY->applyNiceNumbers();
}

// Populates chart with data, right now it's just
// test dummy data.
void ChartWidget::buildCandleChart(int interval) {
   auto chart = ui_->viewPrice->chart();
   priceSeries_->clear();
   volumeSeries_->clear();
   auto product = ui_->cboInstruments->currentText();
   if (product.isEmpty()) {
       product = QStringLiteral("EUR/GBP");
   }
   auto rawData = client_->getRawPointDataArray(product
                                                , static_cast<TradesDB::Interval>(interval));
   for (const auto& dp : rawData) {
       addDataPoint(dp->open, dp->high, dp->low, dp->close, dp->timestamp, dp->volume);
   }
   qDeleteAll(rawData);

   ui_->viewPrice->chart()->addSeries(priceSeries_);
   ui_->viewVolume->chart()->addSeries(volumeSeries_);

   auto candlestickAxisY = qobject_cast<QValueAxis*>(ui_->viewPrice->chart()->axisY());
   if (candlestickAxisY) {
       candlestickAxisY->applyNiceNumbers();
   }
   auto volumeAxisY = qobject_cast<QValueAxis*>(ui_->viewVolume->chart()->axisY());
   if (volumeAxisY) {
       volumeAxisY->applyNiceNumbers();
   }
}

void ChartWidget::addDataPoint(qreal open, qreal high, qreal low, qreal close, qreal timestamp, qreal volume) {
   priceSeries_->append(new CustomCandlestickSet(open, high, low, close, volume, timestamp, priceSeries_));
   volumeSeries_->append(new QCandlestickSet(0.0, volume, 0.0, volume, timestamp, volumeSeries_));
}

// Handles changes of date range.
void ChartWidget::onDateRangeChanged(int id) {
   qDebug() << "clicked" << id;
   auto interval = static_cast<TradesDB::Interval>(id);
   buildCandleChart(interval);

   /*auto candlestickAxisX = qobject_cast<QDateTimeAxis*>(ui_->viewPrice->chart()->axisX(priceSeries_));
   if (candlestickAxisX) {
       switch (static_cast<TradesDB::Interval>(dateRange_.checkedId())) {
       case TradesDB::Interval::OneYear:
       case TradesDB::Interval::SixMonths:
           candlestickAxisX->setTickCount(5);
           break;
       case TradesDB::Interval::OneMonth:
       case TradesDB::Interval::OneWeek:
           candlestickAxisX->setTickCount(10);
           break;
       case TradesDB::Interval::TwentyFourHours:
       case TradesDB::Interval::TwelveHours:
           candlestickAxisX->setTickCount(20);
           break;
       case TradesDB::Interval::SixHours:
       case TradesDB::Interval::OneHour:
           candlestickAxisX->setTickCount(40);
           break;
       default:
           candlestickAxisX->setTickCount(10);
           break;
       }
   }
   auto volumeAxisX = qobject_cast<QDateTimeAxis*>(ui_->viewVolume->chart()->axisX(volumeSeries_));
   if (volumeAxisX) {
       switch (static_cast<TradesDB::Interval>(dateRange_.checkedId())) {
       case TradesDB::Interval::OneYear:
       case TradesDB::Interval::SixMonths:
           volumeAxisX->setTickCount(5);
           break;
       case TradesDB::Interval::OneMonth:
       case TradesDB::Interval::OneWeek:
           volumeAxisX->setTickCount(10);
           break;
       case TradesDB::Interval::TwentyFourHours:
       case TradesDB::Interval::TwelveHours:
           volumeAxisX->setTickCount(20);
           break;
       case TradesDB::Interval::SixHours:
       case TradesDB::Interval::OneHour:
           volumeAxisX->setTickCount(40);
           break;
       default:
           volumeAxisX->setTickCount(10);
           break;
       }
   }*/
}

// This slot function is called when mouse cursor hovers over a candlestick.
void ChartWidget::onPriceHover(bool status, QCandlestickSet *set) {
   if (status) {
      if (set) {
         auto customSet = qobject_cast<CustomCandlestickSet *>(set);
         dataItemText_->setPlainText(QString(tr("O: %1   H: %2   L: %3   C: %4   Volume: %5")
            .arg(customSet->open(), 0, 'g', -1)
            .arg(customSet->high(), 0, 'g', -1)
            .arg(customSet->low(), 0, 'g', -1)
            .arg(customSet->close(), 0, 'g', -1)
            .arg(customSet->volume(), 0, 'g', -1)));
      }
   }
   else {
      dataItemText_->setPlainText(tr(""));
   }
}

void ChartWidget::onInstrumentChanged(const QString &text) {
   ui_->viewPrice->chart()->setTitle(text);
   buildCandleChart(dateRange_.checkedId());
}
