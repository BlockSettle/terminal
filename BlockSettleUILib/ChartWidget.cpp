#include "ChartWidget.h"
#include "spdlog/logger.h"
#include "ui_ChartWidget.h"
#include "Colors.h"
#include "MarketDataProvider.h"
#include "MdhsClient.h"
#include "market_data_history.pb.h"

const qreal BASE_FACTOR = 1.0;

const QColor BACKGROUND_COLOR = QColor(28, 40, 53);
const QColor FOREGROUND_COLOR = QColor(Qt::white);
const QColor VOLUME_COLOR     = QColor(32, 159, 223);

ChartWidget::ChartWidget(QWidget* pParent)
   : QWidget(pParent)
   , ui_(new Ui::ChartWidget)
   , title_(nullptr)
   , info_(nullptr)
   , candlesticksChart_(nullptr)
   , volumeChart_(nullptr)
   , volumeAxisRect_(nullptr) {
   ui_->setupUi(this);

   // setting up date range radio button group
   dateRange_.addButton(ui_->btn1h, Interval::OneHour);
   dateRange_.addButton(ui_->btn6h, Interval::SixHours);
   dateRange_.addButton(ui_->btn12h, Interval::TwelveHours);
   dateRange_.addButton(ui_->btn24h, Interval::TwentyFourHours);
   dateRange_.addButton(ui_->btn1w, Interval::OneWeek);
   dateRange_.addButton(ui_->btn1m, Interval::OneMonth);
   dateRange_.addButton(ui_->btn6m, Interval::SixMonths);
   dateRange_.addButton(ui_->btn1y, Interval::OneYear);
   connect(&dateRange_, qOverload<int>(&QButtonGroup::buttonClicked),
           this, &ChartWidget::OnDateRangeChanged);

   connect(ui_->cboInstruments, &QComboBox::currentTextChanged,
           this, &ChartWidget::OnInstrumentChanged);

   // sort model for instruments combo box
   cboModel_ = new QStandardItemModel(this);
   auto proxy = new QSortFilterProxyModel();
   proxy->setSourceModel(cboModel_);
   proxy->sort(0);
   ui_->cboInstruments->setModel(proxy);
}

void ChartWidget::init(const std::shared_ptr<ApplicationSettings>& appSettings
                       , const std::shared_ptr<MarketDataProvider>& mdProvider
                       , const std::shared_ptr<ConnectionManager>& connectionManager
                       , const std::shared_ptr<spdlog::logger>& logger)
{
	appSettings_ = appSettings;
	mdProvider_ = mdProvider;
	mdhsClient_ = std::make_shared<MdhsClient>(appSettings, connectionManager, logger);
	logger_ = logger;

	connect(mdhsClient_.get(), &MdhsClient::DataReceived, this, &ChartWidget::OnDataReceived);
	connect(mdProvider_.get(), &MarketDataProvider::MDUpdate, this, &ChartWidget::OnMdUpdated);

	MarketDataHistoryRequest request;
	request.set_request_type(MarketDataHistoryMessageType::ProductsListType);
	mdhsClient_->SendRequest(request);

   // initialize charts
   InitializeCustomPlot();

   // initial select interval
   ui_->btn1h->click();
}

ChartWidget::~ChartWidget() {
   delete ui_;
}

// Populate combo box with existing instruments comeing from mdProvider
void ChartWidget::OnMdUpdated(bs::network::Asset::Type assetType, const QString &security, bs::network::MDFields mdFields) {
   auto cbo = ui_->cboInstruments;
   if ((assetType == bs::network::Asset::Undefined) && security.isEmpty()) // Celer disconnected
   {
      cboModel_->clear();
      return;
   }

   //if (cboModel_->findItems(security).isEmpty())
   //{
   //   cboModel_->appendRow(new QStandardItem(security));
   //}
}

void ChartWidget::UpdateChart(const int& interval) const
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
	qreal width = 0.8 * IntervalWidth(interval) / 1000;
	candlesticksChart_->setWidth(width);
	volumeChart_->setWidth(width);

	OhlcRequest ohlcRequest;
	ohlcRequest.set_product(product.toStdString());
	ohlcRequest.set_interval(static_cast<Interval>(interval));
	ohlcRequest.set_limit(100);

	MarketDataHistoryRequest request;
	request.set_request_type(MarketDataHistoryMessageType::OhlcHistoryType);
	request.set_request(ohlcRequest.SerializeAsString());
	mdhsClient_->SendRequest(request);
}

void ChartWidget::OnDataReceived(const std::string& data)
{
	if (data.empty())
	{
		logger_->error("Empty data received from mdhs.");
		return;
	}

	MarketDataHistoryResponse response;
	if (!response.ParseFromString(data))
	{
		logger_->error("can't parse response from mdhs: {}", data);
		return;
	}

	switch (response.response_type()) {
	case MarketDataHistoryMessageType::ProductsListType:
		ProcessProductsListResponse(response.response());
		break;
	case MarketDataHistoryMessageType::OhlcHistoryType:
		ProcessOhlcHistoryResponse(response.response());
		break;
	default:
		logger_->error("[ApiServerConnectionListener::OnDataReceived] undefined message type");
		break;
	}
}

void ChartWidget::ProcessProductsListResponse(const std::string& data)
{
	if (data.empty())
	{
		logger_->error("Empty data received from mdhs.");
		return;
	}

	ProductsListResponse response;
	if (!response.ParseFromString(data))
	{
		logger_->error("can't parse response from mdhs: {}", data);
		return;
	}

	for (const auto& product : response.products())
	{
		cboModel_->appendRow(new QStandardItem(QString::fromStdString(product)));
	}
}

void ChartWidget::ProcessOhlcHistoryResponse(const std::string& data)
{
	if (data.empty())
	{
		logger_->error("Empty data received from mdhs.");
		return;
	}

	OhlcResponse response;
	if (!response.ParseFromString(data))
	{
		logger_->error("can't parse response from mdhs: {}", data);
		return;
	}

	auto product = ui_->cboInstruments->currentText();
	auto interval = dateRange_.checkedId();

	if (!product.compare(QString::fromStdString(response.product())) || interval != response.interval())
		return;

	qreal maxPrice = 0.0;
	qreal minPrice = -1.0;
	qreal maxVolume = 0.0;
	qreal maxTimestamp = -1.0;
	for (int i = 0; i < response.candles_size(); ++i)
	{
		const auto& candle = response.candles(i);
		maxPrice = qMax(maxPrice, candle.high());
		minPrice = minPrice == -1.0 ? candle.low() : qMin(minPrice, candle.low());
		maxVolume = qMax(maxVolume, candle.volume());
		maxTimestamp = qMax(maxTimestamp, static_cast<qreal>(candle.timestamp()));
		AddDataPoint(candle.open(), candle.high(), candle.low(), candle.close(), candle.timestamp(), candle.volume());
		qDebug("Added: %s, open: %f, high: %f, low: %f, close: %f, volume: %f"
			, QDateTime::fromMSecsSinceEpoch(candle.timestamp()).toUTC().toString(Qt::ISODateWithMs).toStdString().c_str()
			, candle.open()
			, candle.high()
			, candle.low()
			, candle.close()
			, candle.volume());
	}

	qDebug("Min price: %f, Max price: %f, Max volume: %f", minPrice, maxPrice, maxVolume);

	auto margin = qMax(maxPrice - minPrice, 0.01) / 10;
	minPrice -= margin;
	maxPrice += margin;
	minPrice = qMax(minPrice, 0.0);

	ui_->customPlot->rescaleAxes();
	qreal size = IntervalWidth(interval, 100);
	qreal upper = maxTimestamp + 0.8 * IntervalWidth(interval) / 2;
	ui_->customPlot->xAxis->setRange(upper / 1000, size / 1000, Qt::AlignRight);
	volumeAxisRect_->axis(QCPAxis::atRight)->setRange(0, maxVolume);
	ui_->customPlot->yAxis2->setRange(minPrice, maxPrice);
	ui_->customPlot->yAxis2->setNumberPrecision(FractionSizeForProduct(product));
	ui_->customPlot->replot();
}

void ChartWidget::AddDataPoint(const qreal& open, const qreal& high, const qreal& low, const qreal& close, const qreal& timestamp, const qreal& volume) const
{
   if (candlesticksChart_) {
      candlesticksChart_->data()->add(QCPFinancialData(timestamp / 1000, open, high, low, close));
   }
   if (volumeChart_) {
      volumeChart_->data()->add(QCPBarsData(timestamp / 1000, volume));
   }
}

qreal ChartWidget::IntervalWidth(int interval, int count) const
{
   if (interval == -1) {
      return 1.0;
   }
   qreal hour = 3600000;
   switch (static_cast<Interval>(interval)) {
   case Interval::OneYear:
      return hour * 8760 * count;
   case Interval::SixMonths:
      return hour * 4320 * count;
   case Interval::OneMonth:
      return hour * 720 * count;
   case Interval::OneWeek:
      return hour * 168 * count;
   case Interval::TwentyFourHours:
      return hour * 24 * count;
   case Interval::TwelveHours:
      return hour * 12 * count;
   case Interval::SixHours:
      return hour * 6 * count;
   case Interval::OneHour:
      return hour * count;
   default:
      return hour * count;
   }
}

int ChartWidget::FractionSizeForProduct(const QString &product) const
{
	auto productType = mdhsClient_->GetProductType(product);
	switch (productType)
	{
	case MdhsClient::ProductTypeFX:
		return 4;
	case MdhsClient::ProductTypeXBT:
		return 2;
	case MdhsClient::ProductTypePrivateMarket:
		return 6;
	default:
		return -1;
	}
}

// Handles changes of date range.
void ChartWidget::OnDateRangeChanged(int interval) {
   qDebug() << "clicked" << interval;
   UpdateChart(interval);
}

void ChartWidget::OnInstrumentChanged(const QString &text) {
   UpdateChart(dateRange_.checkedId());
}

void ChartWidget::OnPlotMouseMove(QMouseEvent *event)
{
   if (!info_) {
      return;
   }
   auto plottable = ui_->customPlot->plottableAt(event->localPos());
   if (plottable) {
      double x = event->localPos().x();
      double width = 0.8 * IntervalWidth(dateRange_.checkedId()) / 1000;
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

void ChartWidget::InitializeCustomPlot()
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
   ui_->customPlot->axisRect()->axis(QCPAxis::atRight)->setNumberFormat(QStringLiteral("f"));
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
   volumeAxisRect_->axis(QCPAxis::atRight)->setTickLabelFont(ui_->customPlot->axisRect()->axis(QCPAxis::atRight)->labelFont());

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

   connect(ui_->customPlot, &QCustomPlot::mouseMove, this, &ChartWidget::OnPlotMouseMove);
}
