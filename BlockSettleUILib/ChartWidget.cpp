#include "ChartWidget.h"
#include "ui_ChartWidget.h"
#include "ApplicationSettings.h"
#include "MarketDataProvider.h"
#include "ArmoryConnection.h"
#include <qrandom.h>
#include "CustomCandlestickSet.h"
#include "TradesClient.h"
#include "DataPointsLocal.h"
#include "Colors.h"

const qreal BASE_FACTOR = 1.0;

const QColor BACKGROUND_COLOR = QColor(28, 40, 53);
const QColor FOREGROUND_COLOR = QColor(Qt::white);
const QColor VOLUME_COLOR     = QColor(32, 159, 223);

ChartWidget::ChartWidget(QWidget *parent)
   : QWidget(parent)
   , ui_(new Ui::ChartWidget)
   , priceSeries_(nullptr)
   , volumeSeries_(nullptr)
   , priceYAxis_(nullptr)
   , volumeYAxis_(nullptr)
   , title(nullptr)
   , candlesticksChart(nullptr)
   , volumeChart(nullptr)
   , volumeAxisRect(nullptr) {
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
   priceSeries_->setMaximumColumnWidth(15);
   priceSeries_->setMinimumColumnWidth(15);
   connect(priceSeries_, &QCandlestickSeries::hovered, this, &ChartWidget::onPriceHover);
   connect(priceSeries_, &QCandlestickSeries::destroyed, [this]() {
      qDebug() << "Price series destroyed";
   });

   volumeSeries_ = new QCandlestickSeries(this);
   volumeSeries_->setIncreasingColor(QColor(32, 159, 223));
   volumeSeries_->setBodyOutlineVisible(false);
   volumeSeries_->setBodyWidth(1.0);
   volumeSeries_->setMaximumColumnWidth(15);
   volumeSeries_->setMinimumColumnWidth(15);
   connect(volumeSeries_, &QCandlestickSeries::destroyed, [this]() {
      qDebug() << "Volume series destroyed";
   });

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
//   buildCandleChart();
   ui_->viewPrice->chart()->addSeries(priceSeries_);
   ui_->viewVolume->chart()->addSeries(volumeSeries_);
   // style the charts
   setChartStyle();
   // setup the chart axis
   createCandleChartAxis();
   createVolumeChartAxis();
   ui_->btn1h->click();

   initializeCustomPlot();
   updateChart();
}

ChartWidget::~ChartWidget() {
   delete ui_;
   priceSeries_->deleteLater();
   volumeSeries_->deleteLater();
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
   auto chart = ui_->viewPrice->chart();
   auto dateAxisx = new QBarCategoryAxis(this);
   chart->addAxis(dateAxisx, Qt::AlignBottom);
   priceSeries_->attachAxis(dateAxisx);
   setupTimeAxis(dateAxisx, false, dateRange_.checkedId());
   updatePriceValueAxis(tr("%06.1f"));
}

// Creates x and y axis for the volume stick chart.
void ChartWidget::createVolumeChartAxis() {
   auto chart = ui_->viewVolume->chart();
   auto dateAxisx = new QBarCategoryAxis(this);
   chart->addAxis(dateAxisx, Qt::AlignBottom);
   volumeSeries_->attachAxis(dateAxisx);
   setupTimeAxis(dateAxisx, true, dateRange_.checkedId());
   updateVolumeValueAxis(tr("%06.1f"));
}

void ChartWidget::setupTimeAxis(QBarCategoryAxis *axis, bool labelsVisible, int interval)
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
//      axis->setFormat(tr("d-M-yy"));
      break;
   case DataPointsLocal::Interval::TwelveHours:
   case DataPointsLocal::Interval::SixHours:
   case DataPointsLocal::Interval::OneHour:
//      axis->setFormat(tr("d-M-yy H:m"));
      break;
   default:
      break;
   }
   // hide grid lines
   axis->setTitleBrush(QBrush(Qt::white));
   axis->setGridLineVisible(false);
   axis->setMinorGridLineVisible(false);
   axis->setLabelsColor(QColor(Qt::white));
//   axis->setTickCount(10);
   axis->setLabelsVisible(labelsVisible);
   // add space offset to the x axis
   // this code will eventually have to be moved to data range handler function
   // and will need to be called when date range is modified
   /*dateAxisx->setMax(dateAxisx->max().addDays(1));
   dateAxisx->setMin(dateAxisx->min().addDays(-1));*/
}

QValueAxis *ChartWidget::createValueAxis(QCandlestickSeries *series
                                          , const QString &labelFormat
                                          , qreal maxValue
                                          , qreal minValue)
{
   qDebug() << "Create axis for" << series << labelFormat << maxValue << minValue;
   QValueAxis *axis = new QValueAxis;
   axis->setLabelFormat(labelFormat);
   axis->setLabelsColor(QColor(Qt::white));
   axis->setGridLineVisible(false);
   axis->setMinorGridLineVisible(false);
   if (series && series->chart()) {
      series->chart()->addAxis(axis, Qt::AlignRight);
      qDebug() << "chart axes" << series->chart()->axes();
      priceSeries_->attachAxis(axis);
      qDebug() << "chart localize" << series->chart()->localizeNumbers();
   }
   if (maxValue > 0) {
      axis->setMax(maxValue);
   }
   if (minValue >= 0) {
      axis->setMin(minValue);
   }
   qDebug() << "before applyNiceNumbers";
   axis->applyNiceNumbers();
   qDebug() << "after applyNiceNumbers";
   return axis;
}

void ChartWidget::updatePriceValueAxis(const QString &labelFormat
                                       , qreal maxValue
                                       , qreal minValue)
{
   if (priceYAxis_) {
      priceSeries_->chart()->removeAxis(priceYAxis_);
      priceYAxis_->deleteLater();
      priceYAxis_ = nullptr;
   }
   priceYAxis_ = createValueAxis(priceSeries_, labelFormat, maxValue, minValue);
}

void ChartWidget::updateVolumeValueAxis(const QString &labelFormat
                                        , qreal maxValue
                                        , qreal minValue)
{
   if (volumeYAxis_) {
      volumeSeries_->chart()->removeAxis(volumeYAxis_);
      volumeYAxis_->deleteLater();
      volumeYAxis_ = nullptr;
   }
   volumeYAxis_ = createValueAxis(volumeSeries_, labelFormat, maxValue, minValue);
   volumeYAxis_->setTickCount(2);
}

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
   QStringList categories;
   for (const auto& dp : rawData) {
      maxPrice = qMax(maxPrice, dp->high);
      minPrice = minPrice == -1.0 ? dp->low : qMin(minPrice, dp->low);
      maxVolume = qMax(maxVolume, dp->volume);
      addDataPoint(dp->open, dp->high, dp->low, dp->close, dp->timestamp, dp->volume);
      categories.append(barLabel(dp->timestamp, interval));
      qDebug("Added: %s, open: %f, high: %f, low: %f, close: %f, volume: %f"
             , QDateTime::fromMSecsSinceEpoch(dp->timestamp).toUTC().toString(Qt::ISODateWithMs).toStdString().c_str()
             , dp->open
             , dp->high
             , dp->low
             , dp->close
             , dp->volume);
   }
   qDeleteAll(rawData);

   qDebug("Min price: %f, Max price: %f, Max volume: %f", minPrice, maxPrice, maxVolume);

   auto margin = qMax(maxPrice - minPrice, 0.01) / 10;
   minPrice -= margin;
   maxPrice += margin;

   auto priceAxis = qobject_cast<QBarCategoryAxis *>(ui_->viewPrice->chart()->axisX());
   setupTimeAxis(priceAxis, false, dateRange_.checkedId());
   if (priceAxis) {
      priceAxis->setCategories(categories);
   }
   auto volumeAxis = qobject_cast<QBarCategoryAxis *>(ui_->viewVolume->chart()->axisX());
   setupTimeAxis(volumeAxis, true, dateRange_.checkedId());
   if (volumeAxis) {
      volumeAxis->setCategories(categories);
   }

   qreal zoomFactor = 2.0;
   setZoomFactor(zoomFactor);

   QString width;
   int w = qCeil(qMax(qMax(qAbs(maxPrice), qAbs(minPrice)), maxVolume));
   int precise = 4;
   if (w > 1) {
      width = QString::number(QString::number(w).length() + precise + 2);
   }
   QString labelTemplate = QStringLiteral("%0") + QStringLiteral("%1.%2f")
         .arg(width)
         .arg(precise);

   updatePriceValueAxis(labelTemplate, maxPrice, minPrice);
   updateVolumeValueAxis(labelTemplate, maxVolume);

   ui_->viewPrice->repaint();
   ui_->viewVolume->repaint();
}

void ChartWidget::updateChart(int interval)
{
   auto product = ui_->cboInstruments->currentText();
   if (product.isEmpty()) {
       product = QStringLiteral("EUR/GBP");
   }
   if (title) {
      title->setText(product);
   }
   if (!candlesticksChart || !volumeChart) {
      return;
   }
   candlesticksChart->data()->clear();
   volumeChart->data()->clear();
   candlesticksChart->setWidth(0.9 * intervalLength(interval));
   volumeChart->setWidth(0.9 * intervalLength(interval));

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

   qDebug("Min price: %f, Max price: %f, Max volume: %f", minPrice, maxPrice, maxVolume);

   auto margin = qMax(maxPrice - minPrice, 0.01) / 10;
   minPrice -= margin;
   maxPrice += margin;

   /*qreal zoomFactor = 2.0;
   setZoomFactor(zoomFactor);*/

   /*QString width;
   int w = qCeil(qMax(qMax(qAbs(maxPrice), qAbs(minPrice)), maxVolume));
   int precise = 4;
   if (w > 1) {
      width = QString::number(QString::number(w).length() + precise + 2);
   }
   QString labelTemplate = QStringLiteral("%0") + QStringLiteral("%1.%2f")
         .arg(width)
         .arg(precise);*/

//   ui_->customPlot->axisRect()->axis(QCPAxis::atRight)
//   volumeAxisRect->axis(QCPAxis::atRight)
   /*updatePriceValueAxis(labelTemplate, maxPrice, minPrice);
   updateVolumeValueAxis(labelTemplate, maxVolume);*/

   ui_->customPlot->rescaleAxes();
   ui_->customPlot->xAxis->scaleRange(getPlotScale(interval), ui_->customPlot->xAxis->range().center());
   ui_->customPlot->yAxis->scaleRange(1.0, ui_->customPlot->yAxis->range().center());
   ui_->customPlot->replot();
}

void ChartWidget::addDataPoint(qreal open, qreal high, qreal low, qreal close, qreal timestamp, qreal volume) {
   priceSeries_->append(new CustomCandlestickSet(open, high, low, close, volume, timestamp, priceSeries_));
   volumeSeries_->append(new QCandlestickSet(0.0, volume, 0.0, volume, timestamp, volumeSeries_));
   if (candlesticksChart) {
      candlesticksChart->data()->add(QCPFinancialData(timestamp / 1000, open, high, low, close));
   }
   if (volumeChart) {
      volumeChart->data()->add(QCPBarsData(timestamp / 1000, volume));
   }
}

qreal ChartWidget::getZoomFactor(int interval) const
{
   if (interval == -1) {
      return 1.0;
   }
   switch (static_cast<DataPointsLocal::Interval>(interval)) {
   case DataPointsLocal::OneYear:
      return BASE_FACTOR * 8760;
   case DataPointsLocal::SixMonths:
      return BASE_FACTOR * 4320;
   case DataPointsLocal::OneMonth:
      return BASE_FACTOR * 720;
   case DataPointsLocal::OneWeek:
      return BASE_FACTOR * 168;
   case DataPointsLocal::TwentyFourHours:
      return BASE_FACTOR * 24;
   case DataPointsLocal::TwelveHours:
      return BASE_FACTOR * 12;
   case DataPointsLocal::SixHours:
      return BASE_FACTOR * 6;
   case DataPointsLocal::OneHour:
      return BASE_FACTOR;
   default:
      return BASE_FACTOR;
   }
}

qreal ChartWidget::getPlotScale(int interval) const
{
   if (interval == -1) {
      return 1.0;
   }
   switch (static_cast<DataPointsLocal::Interval>(interval)) {
   case DataPointsLocal::OneYear:
      return BASE_FACTOR * 8760 / 96;
   case DataPointsLocal::SixMonths:
      return BASE_FACTOR * 4320 / 48;
   case DataPointsLocal::OneMonth:
      return BASE_FACTOR * 720 / 8;
   case DataPointsLocal::OneWeek:
      return BASE_FACTOR * 168 / 2;
   case DataPointsLocal::TwentyFourHours:
      return BASE_FACTOR * 24 / 2;
   case DataPointsLocal::TwelveHours:
      return BASE_FACTOR * 12 / 2;
   case DataPointsLocal::SixHours:
      return BASE_FACTOR * 6 / 2;
   case DataPointsLocal::OneHour:
      return BASE_FACTOR;
   default:
      return BASE_FACTOR;
   }
}

void ChartWidget::setZoomFactor(qreal factor)
{
   qDebug("Update zoom factor: %f", factor);
   ui_->viewPrice->setZoomFactor(factor);
   ui_->viewVolume->setZoomFactor(factor);
}

QString ChartWidget::barLabel(qreal timestamp, int interval) const
{
   QDateTime time = QDateTime::fromMSecsSinceEpoch(timestamp).toUTC();
//   return time.toString(Qt::ISODate);
   switch (static_cast<DataPointsLocal::Interval>(interval)) {
   case DataPointsLocal::OneYear:
      return time.toString(QStringLiteral("yy"));
   case DataPointsLocal::SixMonths:
      return time.toString(QStringLiteral("M"));
   case DataPointsLocal::OneMonth:
      return time.toString(QStringLiteral("M"));
   case DataPointsLocal::OneWeek:
      return QString::number(time.date().weekNumber());
   case DataPointsLocal::TwentyFourHours:
      return time.toString(QStringLiteral("d"));
   case DataPointsLocal::TwelveHours:
      return time.toString(QStringLiteral("H"));
   case DataPointsLocal::SixHours:
      return time.toString(QStringLiteral("H"));
   case DataPointsLocal::OneHour:
      return time.toString(QStringLiteral("hh"));
   default:
      return time.toString(QStringLiteral("HH"));
   }
}

qreal ChartWidget::intervalLength(int interval) const
{
   qreal hour = 3600;
   switch (static_cast<DataPointsLocal::Interval>(interval)) {
   case DataPointsLocal::OneYear:
      return hour * 365 * 24;
   case DataPointsLocal::SixMonths:
      return hour * 183 * 24;
   case DataPointsLocal::OneMonth:
      return hour * 30 * 24;
   case DataPointsLocal::OneWeek:
      return hour * 7 * 24;
   case DataPointsLocal::TwentyFourHours:
      return hour * 24;
   case DataPointsLocal::TwelveHours:
      return hour * 12;
   case DataPointsLocal::SixHours:
      return hour * 6;
   case DataPointsLocal::OneHour:
      return hour;
   default:
      return hour;
   }
}

// Handles changes of date range.
void ChartWidget::onDateRangeChanged(int id) {
   qDebug() << "clicked" << id;
   auto interval = static_cast<DataPointsLocal::Interval>(id);

//   buildCandleChart(interval);
   updateChart(interval);
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

//   buildCandleChart(dateRange_.checkedId());
   updateChart(dateRange_.checkedId());
}

void ChartWidget::initializeCustomPlot()
{
   QBrush bgBrush(BACKGROUND_COLOR);
   ui_->customPlot->setBackground(bgBrush);

   //add title
   title = new QCPTextElement(ui_->customPlot);
   title->setTextColor(FOREGROUND_COLOR);
   title->setFont(QFont(QStringLiteral("sans"), 12));
   ui_->customPlot->plotLayout()->insertRow(0);
   ui_->customPlot->plotLayout()->addElement(0, 0, title);

   // create candlestick chart:
   candlesticksChart = new QCPFinancial(ui_->customPlot->xAxis, ui_->customPlot->yAxis2);
   candlesticksChart->setName(tr("Candlestick"));
   candlesticksChart->setChartStyle(QCPFinancial::csCandlestick);
   candlesticksChart->setTwoColored(true);
   candlesticksChart->setBrushPositive(c_greenColor);
   candlesticksChart->setBrushNegative(c_redColor);
   candlesticksChart->setPenPositive(QPen(c_greenColor));
   candlesticksChart->setPenNegative(QPen(c_redColor));

   ui_->customPlot->axisRect()->axis(QCPAxis::atLeft)->setVisible(false);
   ui_->customPlot->axisRect()->axis(QCPAxis::atRight)->setVisible(true);
   ui_->customPlot->axisRect()->axis(QCPAxis::atRight)->setBasePen(QPen(FOREGROUND_COLOR));
   ui_->customPlot->axisRect()->axis(QCPAxis::atRight)->setTickPen(QPen(FOREGROUND_COLOR));
   ui_->customPlot->axisRect()->axis(QCPAxis::atRight)->setSubTickPen(QPen(FOREGROUND_COLOR));
   ui_->customPlot->axisRect()->axis(QCPAxis::atRight)->setTickLabelColor(FOREGROUND_COLOR);
   ui_->customPlot->axisRect()->axis(QCPAxis::atRight)->setTickLength(0, 8);
   ui_->customPlot->axisRect()->axis(QCPAxis::atRight)->setSubTickLength(0, 4);
   ui_->customPlot->axisRect()->axis(QCPAxis::atBottom)->grid()->setPen(Qt::NoPen);

   // create bottom axis rect for volume bar chart:
   volumeAxisRect = new QCPAxisRect(ui_->customPlot);
   ui_->customPlot->plotLayout()->addElement(2, 0, volumeAxisRect);
   volumeAxisRect->setMaximumSize(QSize(QWIDGETSIZE_MAX, 100));
   volumeAxisRect->axis(QCPAxis::atBottom)->setLayer(QStringLiteral("axes"));
   volumeAxisRect->axis(QCPAxis::atBottom)->grid()->setLayer(QStringLiteral("grid"));
   // bring bottom and main axis rect closer together:
   ui_->customPlot->plotLayout()->setRowSpacing(0);
   volumeAxisRect->setAutoMargins(QCP::msLeft|QCP::msRight|QCP::msBottom);
   volumeAxisRect->setMargins(QMargins(0, 0, 0, 0));
   // create two bar plottables, for positive (green) and negative (red) volume bars:
   ui_->customPlot->setAutoAddPlottableToLegend(false);

   volumeChart = new QCPBars(volumeAxisRect->axis(QCPAxis::atBottom), volumeAxisRect->axis(QCPAxis::atRight));
   volumeChart->setPen(QPen(VOLUME_COLOR));
   volumeChart->setBrush(VOLUME_COLOR);

   volumeAxisRect->axis(QCPAxis::atLeft)->setVisible(false);
   volumeAxisRect->axis(QCPAxis::atRight)->setVisible(true);
   volumeAxisRect->axis(QCPAxis::atRight)->setBasePen(QPen(FOREGROUND_COLOR));
   volumeAxisRect->axis(QCPAxis::atRight)->setTickPen(QPen(FOREGROUND_COLOR));
   volumeAxisRect->axis(QCPAxis::atRight)->setSubTickPen(QPen(FOREGROUND_COLOR));
   volumeAxisRect->axis(QCPAxis::atRight)->setTickLabelColor(FOREGROUND_COLOR);
   volumeAxisRect->axis(QCPAxis::atRight)->setTickLength(0, 8);
   volumeAxisRect->axis(QCPAxis::atRight)->setSubTickLength(0, 4);
   volumeAxisRect->axis(QCPAxis::atRight)->ticker()->setTickCount(1);

   volumeAxisRect->axis(QCPAxis::atBottom)->setBasePen(QPen(FOREGROUND_COLOR));
   volumeAxisRect->axis(QCPAxis::atBottom)->setTickPen(QPen(FOREGROUND_COLOR));
   volumeAxisRect->axis(QCPAxis::atBottom)->setSubTickPen(QPen(FOREGROUND_COLOR));
   volumeAxisRect->axis(QCPAxis::atBottom)->setTickLength(0, 8);
   volumeAxisRect->axis(QCPAxis::atBottom)->setSubTickLength(0, 4);
   volumeAxisRect->axis(QCPAxis::atBottom)->setTickLabelColor(FOREGROUND_COLOR);
   volumeAxisRect->axis(QCPAxis::atBottom)->grid()->setPen(Qt::NoPen);

   // interconnect x axis ranges of main and bottom axis rects:
   connect(ui_->customPlot->xAxis, SIGNAL(rangeChanged(QCPRange))
           , volumeAxisRect->axis(QCPAxis::atBottom), SLOT(setRange(QCPRange)));
   connect(volumeAxisRect->axis(QCPAxis::atBottom), SIGNAL(rangeChanged(QCPRange))
           , ui_->customPlot->xAxis, SLOT(setRange(QCPRange)));
   // configure axes of both main and bottom axis rect:
   QSharedPointer<QCPAxisTickerDateTime> dateTimeTicker(new QCPAxisTickerDateTime);
   dateTimeTicker->setDateTimeSpec(Qt::UTC);
   dateTimeTicker->setDateTimeFormat(QStringLiteral("dd/MM/yy\nHH:mm"));
   dateTimeTicker->setTickCount(20);
   volumeAxisRect->axis(QCPAxis::atBottom)->setTicker(dateTimeTicker);
   volumeAxisRect->axis(QCPAxis::atBottom)->setTickLabelRotation(15);
   ui_->customPlot->xAxis->setBasePen(Qt::NoPen);
   ui_->customPlot->xAxis->setTickLabels(false);
   ui_->customPlot->xAxis->setTicks(false); // only want vertical grid in main axis rect, so hide xAxis backbone, ticks, and labels
   ui_->customPlot->xAxis->setTicker(dateTimeTicker);
   ui_->customPlot->rescaleAxes();
   ui_->customPlot->xAxis->scaleRange(1.025, ui_->customPlot->xAxis->range().center());
   ui_->customPlot->yAxis->scaleRange(1.1, ui_->customPlot->yAxis->range().center());

   // make axis rects' left side line up:
   QCPMarginGroup *group = new QCPMarginGroup(ui_->customPlot);
   ui_->customPlot->axisRect()->setMarginGroup(QCP::msLeft|QCP::msRight, group);
   volumeAxisRect->setMarginGroup(QCP::msLeft|QCP::msRight, group);

   //make draggable horizontally
   ui_->customPlot->setInteraction(QCP::iRangeDrag, true);
   ui_->customPlot->axisRect()->setRangeDrag(Qt::Horizontal);
   volumeAxisRect->setRangeDrag(Qt::Horizontal);
}
