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
   dateRange_.addButton(ui_->btn1h, DataPointsLocal::Interval::OneHour);
   dateRange_.addButton(ui_->btn6h, DataPointsLocal::Interval::SixHours);
   dateRange_.addButton(ui_->btn12h, DataPointsLocal::Interval::TwelveHours);
   dateRange_.addButton(ui_->btn24h, DataPointsLocal::Interval::TwentyFourHours);
   dateRange_.addButton(ui_->btn1w, DataPointsLocal::Interval::OneWeek);
   dateRange_.addButton(ui_->btn1m, DataPointsLocal::Interval::OneMonth);
   dateRange_.addButton(ui_->btn6m, DataPointsLocal::Interval::SixMonths);
   dateRange_.addButton(ui_->btn1y, DataPointsLocal::Interval::OneYear);
   connect(&dateRange_, qOverload<int>(&QButtonGroup::buttonClicked), 
      this, &ChartWidget::onDateRangeChanged);

   // these signals are used to sync volume chart with price chart
   // when scrolling and zooming
   /*connect(ui_->viewPrice, &CustomChartView::chartZoomed,
      ui_->viewVolume, &CustomChartView::onChartZoomed);*/
   connect(ui_->viewPrice, &CustomChartView::chartScrolled,
      ui_->viewVolume, &CustomChartView::onChartScrolled);
   connect(ui_->viewPrice, &CustomChartView::chartZoomReset,
      ui_->viewVolume, &CustomChartView::onChartZoomReset);
   // drag and zoom is enabled only for the price chart
   ui_->viewPrice->enableDrag(true);
//   ui_->viewPrice->enableZoom(true);

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
   priceSeries_->setBodyOutlineVisible(/*false*/true);
   priceSeries_->setBodyWidth(1.0);
   priceSeries_->setMaximumColumnWidth(10);
   priceSeries_->setMinimumColumnWidth(10);
   connect(priceSeries_, &QCandlestickSeries::hovered, this, &ChartWidget::onPriceHover);

   volumeSeries_ = new QCandlestickSeries(this);
   volumeSeries_->setIncreasingColor(QColor(32, 159, 223));
   volumeSeries_->setBodyOutlineVisible(false);
   volumeSeries_->setBodyWidth(1.0);
   volumeSeries_->setMaximumColumnWidth(10);
   volumeSeries_->setMinimumColumnWidth(10);

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
   ui_->viewPrice->chart()->addSeries(priceSeries_);
   ui_->viewVolume->chart()->addSeries(volumeSeries_);
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
   chart->addAxis(dateAxisx, Qt::AlignBottom);
   chart->series().first()->attachAxis(dateAxisx);
   setupTimeAxis(dateAxisx, false, DataPointsLocal::Interval::OneHour);

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
   chart->addAxis(dateAxisx, Qt::AlignBottom);
   chart->series().first()->attachAxis(dateAxisx);
   setupTimeAxis(dateAxisx, true, DataPointsLocal::Interval::OneHour);

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

void ChartWidget::setupTimeAxis(QDateTimeAxis *axis, bool labelsVisible, int interval)
{
   if (interval == -1 || !axis) {
      return;
   }
   switch (static_cast<DataPointsLocal::Interval>(interval)) {
   case DataPointsLocal::Interval::OneYear:
   case DataPointsLocal::Interval::SixMonths:
   case DataPointsLocal::Interval::OneMonth:
   case DataPointsLocal::Interval::OneWeek:
   case DataPointsLocal::Interval::TwentyFourHours:
      axis->setFormat(tr("d-M-yy"));
      break;
   case DataPointsLocal::Interval::TwelveHours:
   case DataPointsLocal::Interval::SixHours:
   case DataPointsLocal::Interval::OneHour:
      axis->setFormat(tr("d-M-yy H:m"));
      break;
   default:
      break;
   }
   // hide grid lines
   axis->setTitleBrush(QBrush(Qt::white));
   axis->setGridLineVisible(false);
   axis->setMinorGridLineVisible(false);
   axis->setLabelsColor(QColor(Qt::white));
   axis->setTickCount(10);
   axis->setLabelsVisible(labelsVisible);
   // add space offset to the x axis
   // this code will eventually have to be moved to data range handler function
   // and will need to be called when date range is modified
   /*dateAxisx->setMax(dateAxisx->max().addDays(1));
   dateAxisx->setMin(dateAxisx->min().addDays(-1));*/
}

// Populates chart with data, right now it's just
// test dummy data.
void ChartWidget::buildCandleChart(int interval) {
   priceSeries_->clear();
   volumeSeries_->clear();
   auto product = ui_->cboInstruments->currentText();
   if (product.isEmpty()) {
       product = QStringLiteral("EUR/GBP");
   }
   auto rawData = client_->getRawPointDataArray(product
                                                , static_cast<DataPointsLocal::Interval>(interval));
   qreal maxPrice = 0.0;
   qreal minPrice = -1.0;
   qreal maxVolume = 0.0;
   for (const auto& dp : rawData) {
      maxPrice = qMax(maxPrice, dp->high);
      minPrice = minPrice == -1.0 ? dp->low : qMin(minPrice, dp->low);
      maxVolume = qMax(maxVolume, dp->volume);
      addDataPoint(dp->open, dp->high, dp->low, dp->close, dp->timestamp, dp->volume);
      qDebug("Added: %s, open: %f, high: %f, low: %f, close: %f, volume: %f"
             , QDateTime::fromMSecsSinceEpoch(dp->timestamp).toUTC().toString(Qt::ISODateWithMs).toStdString().c_str()
             , dp->open
             , dp->high
             , dp->low
             , dp->close
             , dp->volume);
   }
   qDeleteAll(rawData);
   auto margin = qMax(maxPrice - minPrice, 0.01) / 10;
   minPrice -= margin;
   maxPrice += margin;

   qDebug("Min price: %f, Max price: %f, Max volume: %f", minPrice, maxPrice, maxVolume);
   auto candlestickAxisY = qobject_cast<QValueAxis*>(ui_->viewPrice->chart()->axisY());
   if (candlestickAxisY) {
      candlestickAxisY->setRange(minPrice, maxPrice);
//      candlestickAxisY->applyNiceNumbers();
   }
   auto volumeAxisY = qobject_cast<QValueAxis*>(ui_->viewVolume->chart()->axisY());
   if (volumeAxisY) {
      volumeAxisY->setMax(maxVolume);
//      volumeAxisY->applyNiceNumbers();
   }

   qreal zoomFactor = getZoomFactor(interval);
   qDebug("Update zoom factor: %f", zoomFactor);
   ui_->viewPrice->setZoomFactor(zoomFactor);
   ui_->viewVolume->setZoomFactor(zoomFactor);
}

void ChartWidget::addDataPoint(qreal open, qreal high, qreal low, qreal close, qreal timestamp, qreal volume) {
   priceSeries_->append(new CustomCandlestickSet(open, high, low, close, volume, timestamp, priceSeries_));
   volumeSeries_->append(new QCandlestickSet(0.0, volume, 0.0, volume, timestamp, volumeSeries_));
}

qreal ChartWidget::getZoomFactor(int interval)
{
   if (interval == -1) {
      return 1.0;
   }
   switch (static_cast<DataPointsLocal::Interval>(interval)) {
   case DataPointsLocal::OneYear:
      return 24.0 * 365; //0.0001;
   case DataPointsLocal::SixMonths:
      return 24.0 * 30 * 6; //0.0002;
   case DataPointsLocal::OneMonth:
      return 24.0 * 30; //0.0015;
   case DataPointsLocal::OneWeek:
      return 24.0 * 7.0; //0.006;
   case DataPointsLocal::TwentyFourHours:
      return 24.0; //0.042;
   case DataPointsLocal::TwelveHours:
      return 12.0; //0.083;
   case DataPointsLocal::SixHours:
      return 6.0; //0.16;
   case DataPointsLocal::OneHour:
      return 1.0;
   default:
      return 1.0;
   }
}

// Handles changes of date range.
void ChartWidget::onDateRangeChanged(int id) {
   qDebug() << "clicked" << id;
   auto interval = static_cast<DataPointsLocal::Interval>(id);
   buildCandleChart(interval);
   auto priceAxis = qobject_cast<QDateTimeAxis *>(ui_->viewPrice->chart()->axisX());
   setupTimeAxis(priceAxis, false, dateRange_.checkedId());
   auto volumeAxis = qobject_cast<QDateTimeAxis *>(ui_->viewVolume->chart()->axisX());
   setupTimeAxis(volumeAxis, true, dateRange_.checkedId());
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
