#include "ChartWidget.h"
#include "ui_ChartWidget.h"
#include "ApplicationSettings.h"
#include "MarketDataProvider.h"
#include "ArmoryConnection.h"

ChartWidget::ChartWidget(QWidget *parent)
   : QWidget(parent)
   , ui_(new Ui::ChartWidget) {
   ui_->setupUi(this);

   // setting up date range radio button group
   dateRange_.addButton(ui_->btn1h, 1);
   dateRange_.addButton(ui_->btn6h, 2);
   dateRange_.addButton(ui_->btn12h, 3);
   dateRange_.addButton(ui_->btn24h, 4);
   dateRange_.addButton(ui_->btn1w, 5);
   dateRange_.addButton(ui_->btn1m, 6);
   dateRange_.addButton(ui_->btn6m, 7);
   dateRange_.addButton(ui_->btn1y, 8);
   connect(&dateRange_, qOverload<int>(&QButtonGroup::buttonClicked), 
      this, &ChartWidget::onDateRangeChanged);

   // sort model for instruments combo box
   cboModel_ = new QStandardItemModel(this);
   auto proxy = new QSortFilterProxyModel();
   proxy->setSourceModel(cboModel_);
   proxy->sort(0);
   ui_->cboInstruments->setModel(proxy);

   ui_->viewMain->setRubberBand(QChartView::HorizontalRubberBand);
}

void ChartWidget::init(const std::shared_ptr<ApplicationSettings> &, const std::shared_ptr<MarketDataProvider> &mdProvider
   , const std::shared_ptr<ArmoryConnection> &) {
   mdProvider_ = mdProvider;


   connect(mdProvider.get(), &MarketDataProvider::MDUpdate, this, &ChartWidget::onMDUpdated);

   //populateInstruments();

   // populate charts with data
   buildCandleChart();
   buildVolumeChart();
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
   auto mainChart = ui_->viewMain->chart();
   mainChart->setBackgroundBrush(bgBrush);
   // Customize chart title
   QFont font;
   font.setPixelSize(18);
   mainChart->setTitleFont(font);
   mainChart->setTitleBrush(QBrush(Qt::white));
   mainChart->legend()->setVisible(false);

   // volume chart
   auto volChart = ui_->viewVolume->chart();
   volChart->setBackgroundBrush(bgBrush);
   // remove top and bottom margins for the volume chart
   auto margins = volChart->margins();
   margins.setTop(0);
   margins.setBottom(0);
   volChart->setMargins(margins);
   volChart->legend()->setVisible(false);
}

// Creates x and y axis for the candle stick chart.
void ChartWidget::createCandleChartAxis() {
   auto dateAxisx = new QDateTimeAxis;
   auto chart = ui_->viewMain->chart();
   auto series = qobject_cast<QCandlestickSeries *>(chart->series().at(0));
   dateAxisx->setFormat(tr("d-M-yy"));
   //dateAxisx->setTitleText(tr("Date"));
   dateAxisx->setTitleBrush(QBrush(Qt::white));
   // hide grid lines
   dateAxisx->setGridLineVisible(false);
   dateAxisx->setMinorGridLineVisible(false);
   dateAxisx->setLabelsColor(QColor(Qt::white));
   chart->addAxis(dateAxisx, Qt::AlignBottom);
   chart->series().at(0)->attachAxis(dateAxisx);
   dateAxisx->setTickCount(10);
   qDebug() << "count" << series->count();
   QValueAxis *axisY = new QValueAxis;
   axisY->setLabelFormat(tr("%i"));
   axisY->setLabelsColor(QColor(Qt::white));
   axisY->setGridLineVisible(false);
   axisY->setMinorGridLineVisible(false);
   chart->addAxis(axisY, Qt::AlignRight);
   chart->series().at(0)->attachAxis(axisY);
}

// Creates x and y axis for the volume stick chart.
void ChartWidget::createVolumeChartAxis() {
   auto dateAxisx = new QDateTimeAxis;
   auto chart = ui_->viewVolume->chart();
   auto series = qobject_cast<QBarSeries *>(chart->series().at(0));
   auto barset = series->barSets().at(0);
   dateAxisx->setFormat(tr("d"));
   // hide grid lines
   dateAxisx->setGridLineVisible(false);
   dateAxisx->setMinorGridLineVisible(false);
   dateAxisx->setLabelsColor(QColor(Qt::white));
   chart->addAxis(dateAxisx, Qt::AlignBottom);
   chart->series().at(0)->attachAxis(dateAxisx);
   dateAxisx->setTickCount(10);
   dateAxisx->setLabelsVisible(false);

   QValueAxis *axisY = new QValueAxis;
   axisY->setLabelFormat(tr("%i"));
   axisY->setLabelsColor(QColor(Qt::white));
   axisY->setGridLineVisible(false);
   axisY->setMinorGridLineVisible(false);
   axisY->setTickCount(2);
   chart->addAxis(axisY, Qt::AlignRight);
   chart->series().at(0)->attachAxis(axisY);
}

// Populates chart with data, right now it's just
// test dummy data.
void ChartWidget::buildCandleChart() {
   auto chart = ui_->viewMain->chart();
   QCandlestickSeries *series = new QCandlestickSeries();
   series->setIncreasingColor(QColor(34, 192, 100));
   series->setDecreasingColor(QColor(207, 41, 46));
   QPen pen(QRgb(0xffffff));
   pen.setWidth(1);
   series->setPen(pen);
   chart->setTitle(tr("XBT/USD"));
   series->setBodyOutlineVisible(false);

   QDateTime dt;
   dt.setDate(QDate(2018, 10, 1));
   series->append(new QCandlestickSet(12.0, 15.0, 11.0, 13.0, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.3, 13.6, 10.5, 10.6, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(9.4, 9.7, 8.3, 8.2, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(8.6, 9.7, 7.9, 8.2, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(9.4, 11.0, 9.9, 10.1, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(10.6, 13.2, 8.0, 10.9, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(11.2, 12.2, 10.6, 10.6, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.5, 13.2, 9.5, 11.0, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.5, 12.2, 15.8, 15.7, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.5, 12.2, 9.9, 10.0, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.0, 15.0, 11.0, 13.0, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.3, 13.6, 10.5, 10.6, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(9.4, 9.7, 8.3, 8.2, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(8.6, 9.7, 7.9, 8.2, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(9.4, 11.0, 9.9, 10.1, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(10.6, 13.2, 8.0, 10.9, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(11.2, 12.2, 10.6, 10.6, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.5, 13.2, 9.5, 11.0, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.5, 12.2, 9.9, 15.7, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.5, 12.2, 9.9, 10.0, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.0, 15.0, 11.0, 13.0, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.3, 13.6, 10.5, 10.6, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(9.4, 9.7, 8.3, 8.2, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(8.6, 9.7, 7.9, 8.2, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(9.4, 11.0, 9.9, 10.1, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(10.6, 13.2, 8.0, 10.9, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(11.2, 12.2, 10.6, 10.6, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.5, 13.2, 9.5, 11.0, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.5, 18.2, 9.9, 17.7, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.5, 18.2, 9.9, 17.7, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.5, 12.2, 9.9, 10.0, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.0, 15.0, 11.0, 13.0, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.3, 13.6, 10.5, 10.6, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(9.4, 9.7, 8.3, 8.2, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(8.6, 9.7, 7.9, 8.2, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(9.4, 11.0, 9.9, 10.1, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(10.6, 13.2, 8.0, 10.9, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(11.2, 12.2, 10.6, 10.6, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.5, 13.2, 9.5, 11.0, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.5, 12.2, 15.8, 15.7, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.5, 12.2, 9.9, 10.0, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.0, 15.0, 11.0, 13.0, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.3, 13.6, 10.5, 10.6, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(9.4, 9.7, 8.3, 8.2, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(8.6, 9.7, 7.9, 8.2, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(9.4, 11.0, 9.9, 10.1, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(10.6, 13.2, 8.0, 10.9, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(11.2, 12.2, 10.6, 10.6, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.5, 13.2, 9.5, 11.0, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.5, 12.2, 15.8, 15.7, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.5, 12.2, 9.9, 10.0, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.0, 15.0, 11.0, 13.0, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.3, 13.6, 10.5, 10.6, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(9.4, 9.7, 8.3, 8.2, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(8.6, 9.7, 7.9, 8.2, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(9.4, 11.0, 9.9, 10.1, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(10.6, 13.2, 8.0, 10.9, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(11.2, 12.2, 10.6, 10.6, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.5, 13.2, 9.5, 11.0, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.5, 12.2, 15.8, 15.7, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.5, 12.2, 9.9, 10.0, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.0, 15.0, 11.0, 13.0, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.3, 13.6, 10.5, 10.6, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(9.4, 9.7, 8.3, 8.2, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(8.6, 9.7, 7.9, 8.2, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(9.4, 11.0, 9.9, 10.1, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(10.6, 13.2, 8.0, 10.9, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(11.2, 12.2, 10.6, 10.6, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.5, 13.2, 9.5, 11.0, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.5, 12.2, 15.8, 15.7, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.5, 12.2, 9.9, 10.0, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.0, 15.0, 11.0, 13.0, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.3, 13.6, 10.5, 10.6, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(9.4, 9.7, 8.3, 8.2, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(8.6, 9.7, 7.9, 8.2, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(9.4, 11.0, 9.9, 10.1, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(10.6, 13.2, 8.0, 10.9, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(11.2, 12.2, 10.6, 10.6, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.5, 13.2, 9.5, 11.0, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.5, 12.2, 15.8, 15.7, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.5, 12.2, 9.9, 10.0, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.0, 15.0, 11.0, 13.0, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.3, 13.6, 10.5, 10.6, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(9.4, 9.7, 8.3, 8.2, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(8.6, 9.7, 7.9, 8.2, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(9.4, 11.0, 9.9, 10.1, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(10.6, 13.2, 8.0, 10.9, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(11.2, 12.2, 10.6, 10.6, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.5, 13.2, 9.5, 11.0, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.5, 12.2, 15.8, 15.7, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.5, 12.2, 9.9, 10.0, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.0, 15.0, 11.0, 13.0, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.3, 13.6, 10.5, 10.6, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(9.4, 9.7, 8.3, 8.2, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(8.6, 9.7, 7.9, 8.2, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(9.4, 11.0, 9.9, 10.1, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(10.6, 13.2, 8.0, 10.9, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(11.2, 12.2, 10.6, 10.6, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.5, 13.2, 9.5, 11.0, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.5, 12.2, 15.8, 15.7, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.5, 12.2, 9.9, 10.0, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.0, 15.0, 11.0, 13.0, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.3, 13.6, 10.5, 10.6, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(9.4, 9.7, 8.3, 8.2, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(8.6, 9.7, 7.9, 8.2, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(9.4, 11.0, 9.9, 10.1, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(10.6, 13.2, 8.0, 10.9, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(11.2, 12.2, 10.6, 10.6, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.5, 13.2, 9.5, 11.0, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.5, 12.2, 15.8, 15.7, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(9.4, 9.7, 8.3, 8.2, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(8.6, 9.7, 7.9, 8.2, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(9.4, 11.0, 9.9, 10.1, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(10.6, 13.2, 8.0, 10.9, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(11.2, 12.2, 10.6, 10.6, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.5, 13.2, 9.5, 11.0, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.5, 12.2, 15.8, 15.7, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(9.4, 9.7, 8.3, 8.2, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(8.6, 9.7, 7.9, 8.2, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(9.4, 11.0, 9.9, 10.1, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(10.6, 13.2, 8.0, 10.9, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(11.2, 12.2, 10.6, 10.6, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.5, 13.2, 9.5, 11.0, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.5, 12.2, 15.8, 15.7, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(9.4, 9.7, 8.3, 8.2, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(8.6, 9.7, 7.9, 8.2, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(9.4, 11.0, 9.9, 10.1, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(10.6, 13.2, 8.0, 10.9, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(11.2, 12.2, 10.6, 10.6, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.5, 13.2, 9.5, 11.0, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.5, 12.2, 15.8, 15.7, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(11.2, 12.2, 10.6, 10.6, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.5, 13.2, 9.5, 11.0, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.5, 12.2, 15.8, 15.7, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(9.4, 9.7, 8.3, 8.2, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(8.6, 9.7, 7.9, 8.2, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(9.4, 11.0, 9.9, 10.1, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(10.6, 13.2, 8.0, 10.9, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(11.2, 12.2, 10.6, 10.6, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.5, 13.2, 9.5, 11.0, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.5, 12.2, 15.8, 15.7, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(11.2, 12.2, 10.6, 10.6, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.5, 13.2, 9.5, 11.0, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.5, 12.2, 15.8, 15.7, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(9.4, 9.7, 8.3, 8.2, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(8.6, 9.7, 7.9, 8.2, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(9.4, 11.0, 9.9, 10.1, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(10.6, 13.2, 8.0, 10.9, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(11.2, 12.2, 10.6, 10.6, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.5, 13.2, 9.5, 11.0, dt.toMSecsSinceEpoch(), series));
   dt = dt.addDays(1);
   series->append(new QCandlestickSet(12.5, 12.2, 15.8, 15.7, dt.toMSecsSinceEpoch(), series));

   chart->addSeries(series);

}

// Populates volume chart with data.
void ChartWidget::buildVolumeChart() {
   QBarSeries *series = new QBarSeries();
   QBarSet *barSet = new QBarSet(tr("Volume"));
   barSet->setPen(QPen(QRgb(0x209fdf)));
   for (int i = 1; i < 151; i++) {
      barSet->append(i);
   }
   series->append(barSet);

   auto chart = ui_->viewVolume->chart();
   chart->addSeries(series);
}

// Populates instruments combo box.
void ChartWidget::populateInstruments() {
   ui_->cboInstruments->addItem(tr("XBT/USD"));
   ui_->cboInstruments->addItem(tr("XBT/EUR"));
   ui_->cboInstruments->addItem(tr("XBT/GBT"));
   ui_->cboInstruments->addItem(tr("XBT/JPY"));
}

// Handles changes of date range.
void ChartWidget::onDateRangeChanged(int id) {
   qDebug() << "clicked" << id;
}
