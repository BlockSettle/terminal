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
   , title_(nullptr)
   , info_(nullptr)
   , candlesticksChart_(nullptr)
   , volumeChart_(nullptr)
   , volumeAxisRect_(nullptr) {
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

   connect(ui_->cboInstruments, &QComboBox::currentTextChanged,
           this, &ChartWidget::onInstrumentChanged);

   // sort model for instruments combo box
   cboModel_ = new QStandardItemModel(this);
   auto proxy = new QSortFilterProxyModel();
   proxy->setSourceModel(cboModel_);
   proxy->sort(0);
   ui_->cboInstruments->setModel(proxy);
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

   // initialize charts
   initializeCustomPlot();

   // initial select interval
   ui_->btn1h->click();
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

void ChartWidget::updatePriceValueAxis(const QString &labelFormat
                                       , qreal maxValue
                                       , qreal minValue)
{
   /*if (priceYAxis_) {
      priceSeries_->chart()->removeAxis(priceYAxis_);
      priceYAxis_->deleteLater();
      priceYAxis_ = nullptr;
   }
   priceYAxis_ = createValueAxis(priceSeries_, labelFormat, maxValue, minValue);*/
}

void ChartWidget::updateVolumeValueAxis(const QString &labelFormat
                                        , qreal maxValue
                                        , qreal minValue)
{
   /*if (volumeYAxis_) {
      volumeSeries_->chart()->removeAxis(volumeYAxis_);
      volumeYAxis_->deleteLater();
      volumeYAxis_ = nullptr;
   }
   volumeYAxis_ = createValueAxis(volumeSeries_, labelFormat, maxValue, minValue);
   volumeYAxis_->setTickCount(2);*/
}

void ChartWidget::updateChart(int interval)
{
   auto product = ui_->cboInstruments->currentText();
   if (product.isEmpty()) {
      product = QStringLiteral("EUR/GBP");
   }
   if (title_) {
      title_->setText(product);
   }
   if (!candlesticksChart_ || !volumeChart_) {
      return;
   }
   candlesticksChart_->data()->clear();
   volumeChart_->data()->clear();
   qreal width = 0.8 * intervalWidth(interval) / 1000;
   candlesticksChart_->setWidth(width);
   volumeChart_->setWidth(width);

   auto rawData = client_->getRawPointDataArray(product
                                                , static_cast<DataPointsLocal::Interval>(interval));
   qreal maxPrice = 0.0;
   qreal minPrice = -1.0;
   qreal maxVolume = 0.0;
   qreal maxTimestamp = -1.0;
   for (const auto& dp : rawData) {
      maxPrice = qMax(maxPrice, dp->high);
      minPrice = minPrice == -1.0 ? dp->low : qMin(minPrice, dp->low);
      maxVolume = qMax(maxVolume, dp->volume);
      maxTimestamp = qMax(maxTimestamp, dp->timestamp);
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
   minPrice = qMax(minPrice, 0.0);

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

   ui_->customPlot->rescaleAxes();
   qreal size = intervalWidth(interval, 100);
   qreal upper = maxTimestamp + 0.8 * intervalWidth(interval) / 2;
   ui_->customPlot->xAxis->setRange(upper / 1000, size / 1000, Qt::AlignRight);
   volumeAxisRect_->axis(QCPAxis::atRight)->setRange(0, maxVolume);
   ui_->customPlot->yAxis2->setRange(minPrice, maxPrice);
   ui_->customPlot->replot();
}

void ChartWidget::addDataPoint(qreal open, qreal high, qreal low, qreal close, qreal timestamp, qreal volume) {
   if (candlesticksChart_) {
      candlesticksChart_->data()->add(QCPFinancialData(timestamp / 1000, open, high, low, close));
   }
   if (volumeChart_) {
      volumeChart_->data()->add(QCPBarsData(timestamp / 1000, volume));
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

qreal ChartWidget::intervalWidth(int interval, int count) const
{
   if (interval == -1) {
      return 1.0;
   }
   qreal hour = 3600000;
   switch (static_cast<DataPointsLocal::Interval>(interval)) {
   case DataPointsLocal::OneYear:
      return hour * 8760 * count;
   case DataPointsLocal::SixMonths:
      return hour * 4320 * count;
   case DataPointsLocal::OneMonth:
      return hour * 720 * count;
   case DataPointsLocal::OneWeek:
      return hour * 168 * count;
   case DataPointsLocal::TwentyFourHours:
      return hour * 24 * count;
   case DataPointsLocal::TwelveHours:
      return hour * 12 * count;
   case DataPointsLocal::SixHours:
      return hour * 6 * count;
   case DataPointsLocal::OneHour:
      return hour * count;
   default:
      return hour * count;
   }
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

// Handles changes of date range.
void ChartWidget::onDateRangeChanged(int id) {
   qDebug() << "clicked" << id;
   auto interval = static_cast<DataPointsLocal::Interval>(id);

   updateChart(interval);
}

void ChartWidget::onInstrumentChanged(const QString &text) {
   updateChart(dateRange_.checkedId());
}

void ChartWidget::onPlotMouseMove(QMouseEvent *event)
{
   if (!info_) {
      return;
   }
   auto plottable = ui_->customPlot->plottableAt(event->localPos());
   if (plottable) {
      double x = event->localPos().x();
      double width = 0.8 * intervalWidth(dateRange_.checkedId()) / 1000;
      double timestamp = ui_->customPlot->xAxis->pixelToCoord(x) + width / 2;
      auto ohlcValue = *candlesticksChart_->data()->findBegin(timestamp);
      auto volumeValue = *volumeChart_->data()->findBegin(timestamp);
      /*auto date = QDateTime::fromMSecsSinceEpoch(timestamp * 1000).toUTC();
      qDebug() << "Position:" << event->pos() << event->localPos()
               << "Item at:"  << QString::number(timestamp, 'f') << date
               << ohlcValue.key << ohlcValue.open << ohlcValue.high
               << ohlcValue.low << ohlcValue.close << volumeValue.value;*/
      info_->setText(tr("O: %1   H: %2   L: %3   C: %4   Volume: %5")
                     .arg(ohlcValue.open, 0, 'g', -1)
                     .arg(ohlcValue.high, 0, 'g', -1)
                     .arg(ohlcValue.low, 0, 'g', -1)
                     .arg(ohlcValue.close, 0, 'g', -1)
                     .arg( volumeValue.value, 0, 'g', -1));
   } else {
      info_->setText({});
   }
   ui_->customPlot->replot();
}

void ChartWidget::initializeCustomPlot()
{
   QBrush bgBrush(BACKGROUND_COLOR);
   ui_->customPlot->setBackground(bgBrush);

   //add title
   title_ = new QCPTextElement(ui_->customPlot);
   title_->setTextColor(FOREGROUND_COLOR);
   title_->setFont(QFont(QStringLiteral("sans"), 12));
   ui_->customPlot->plotLayout()->insertRow(0);
   ui_->customPlot->plotLayout()->addElement(0, 0, title_);
   //add info
   info_ = new QCPTextElement(ui_->customPlot);
   info_->setTextColor(FOREGROUND_COLOR);
   info_->setFont(QFont(QStringLiteral("sans"), 10));
   info_->setTextFlags(Qt::AlignLeft | Qt::AlignVCenter);
   ui_->customPlot->plotLayout()->insertRow(1);
   ui_->customPlot->plotLayout()->addElement(1, 0, info_);

   // create candlestick chart:
   candlesticksChart_ = new QCPFinancial(ui_->customPlot->xAxis, ui_->customPlot->yAxis2);
   candlesticksChart_->setName(tr("Candlestick"));
   candlesticksChart_->setChartStyle(QCPFinancial::csCandlestick);
   candlesticksChart_->setTwoColored(true);
   candlesticksChart_->setBrushPositive(c_greenColor);
   candlesticksChart_->setBrushNegative(c_redColor);
   candlesticksChart_->setPenPositive(QPen(c_greenColor));
   candlesticksChart_->setPenNegative(QPen(c_redColor));

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
   volumeAxisRect_ = new QCPAxisRect(ui_->customPlot);
   ui_->customPlot->plotLayout()->addElement(3, 0, volumeAxisRect_);
   volumeAxisRect_->setMaximumSize(QSize(QWIDGETSIZE_MAX, 100));
   volumeAxisRect_->axis(QCPAxis::atBottom)->setLayer(QStringLiteral("axes"));
   volumeAxisRect_->axis(QCPAxis::atBottom)->grid()->setLayer(QStringLiteral("grid"));
   // bring bottom and main axis rect closer together:
   ui_->customPlot->plotLayout()->setRowSpacing(0);
   volumeAxisRect_->setAutoMargins(QCP::msLeft|QCP::msRight|QCP::msBottom);
   volumeAxisRect_->setMargins(QMargins(0, 0, 0, 0));
   // create two bar plottables, for positive (green) and negative (red) volume bars:
   ui_->customPlot->setAutoAddPlottableToLegend(false);

   volumeChart_ = new QCPBars(volumeAxisRect_->axis(QCPAxis::atBottom), volumeAxisRect_->axis(QCPAxis::atRight));
   volumeChart_->setPen(QPen(VOLUME_COLOR));
   volumeChart_->setBrush(VOLUME_COLOR);

   volumeAxisRect_->axis(QCPAxis::atLeft)->setVisible(false);
   volumeAxisRect_->axis(QCPAxis::atRight)->setVisible(true);
   volumeAxisRect_->axis(QCPAxis::atRight)->setBasePen(QPen(FOREGROUND_COLOR));
   volumeAxisRect_->axis(QCPAxis::atRight)->setTickPen(QPen(FOREGROUND_COLOR));
   volumeAxisRect_->axis(QCPAxis::atRight)->setSubTickPen(QPen(FOREGROUND_COLOR));
   volumeAxisRect_->axis(QCPAxis::atRight)->setTickLabelColor(FOREGROUND_COLOR);
   volumeAxisRect_->axis(QCPAxis::atRight)->setTickLength(0, 8);
   volumeAxisRect_->axis(QCPAxis::atRight)->setSubTickLength(0, 4);
   volumeAxisRect_->axis(QCPAxis::atRight)->ticker()->setTickCount(1);

   volumeAxisRect_->axis(QCPAxis::atBottom)->setBasePen(QPen(FOREGROUND_COLOR));
   volumeAxisRect_->axis(QCPAxis::atBottom)->setTickPen(QPen(FOREGROUND_COLOR));
   volumeAxisRect_->axis(QCPAxis::atBottom)->setSubTickPen(QPen(FOREGROUND_COLOR));
   volumeAxisRect_->axis(QCPAxis::atBottom)->setTickLength(0, 8);
   volumeAxisRect_->axis(QCPAxis::atBottom)->setSubTickLength(0, 4);
   volumeAxisRect_->axis(QCPAxis::atBottom)->setTickLabelColor(FOREGROUND_COLOR);
   volumeAxisRect_->axis(QCPAxis::atBottom)->grid()->setPen(Qt::NoPen);

   // interconnect x axis ranges of main and bottom axis rects:
   connect(ui_->customPlot->xAxis, SIGNAL(rangeChanged(QCPRange))
           , volumeAxisRect_->axis(QCPAxis::atBottom), SLOT(setRange(QCPRange)));
   connect(volumeAxisRect_->axis(QCPAxis::atBottom), SIGNAL(rangeChanged(QCPRange))
           , ui_->customPlot->xAxis, SLOT(setRange(QCPRange)));
   // configure axes of both main and bottom axis rect:
   QSharedPointer<QCPAxisTickerDateTime> dateTimeTicker(new QCPAxisTickerDateTime);
   dateTimeTicker->setDateTimeSpec(Qt::UTC);
   dateTimeTicker->setDateTimeFormat(QStringLiteral("dd/MM/yy\nHH:mm"));
   dateTimeTicker->setTickCount(20);
   volumeAxisRect_->axis(QCPAxis::atBottom)->setTicker(dateTimeTicker);
   volumeAxisRect_->axis(QCPAxis::atBottom)->setTickLabelRotation(15);
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
   volumeAxisRect_->setMarginGroup(QCP::msLeft|QCP::msRight, group);

   //make draggable horizontally
   ui_->customPlot->setInteraction(QCP::iRangeDrag, true);
   ui_->customPlot->axisRect()->setRangeDrag(Qt::Horizontal);
   volumeAxisRect_->setRangeDrag(Qt::Horizontal);

   connect(ui_->customPlot, &QCustomPlot::mouseMove, this, &ChartWidget::onPlotMouseMove);
}
