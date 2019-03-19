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
   , volumeAxisRect_(nullptr)
   , lastHigh(0.0)
   , lastLow(0.0)
   , lastClose(0.0)
   , currentTimestamp(0.0)
   , maxPrice(0.0)
   , minPrice(0.0)
   , timerId(0)
   , lastInterval(-1)
   , dragY(0)
   , isDraggingYAxis(false) {
   ui_->setupUi(this);
   setAutoScaleBtnColor();
   connect(ui_->autoScaleBtn, &QPushButton::clicked, [this]() {
	   if (autoScaling = !autoScaling)
	   {
		   rescalePlot();
	   }
	   setAutoScaleBtnColor();
   });
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
   killTimer(timerId);
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

   if (title_->text() == security)
   {
      for (const auto& field : mdFields)
      {
		 if (field.type == bs::network::MDField::PriceLast)
		 {
			 if (field.value == lastClose)
				 return;
			 else
				 lastClose = field.value;

			 if (lastClose > lastHigh)
				 lastHigh = lastClose;

			 if (lastClose < lastLow)
				 lastLow = lastClose;
		 }

		 if (field.type == bs::network::MDField::MDTimestamp)
		 {
			 currentTimestamp = field.value;
			 timerId = startTimer(getTimerInterval());
		 }
      }
   }

   ModifyCandle();

   //if (cboModel_->findItems(security).isEmpty())
   //{
   //   cboModel_->appendRow(new QStandardItem(security));
   //}
}

void ChartWidget::UpdateChart(const int& interval) const
{
   auto product = ui_->cboInstruments->currentText();
   if (product.isEmpty())
      return;
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
   const auto currentTimestamp = QDateTime::currentMSecsSinceEpoch();
   OhlcRequest ohlcRequest;
   ohlcRequest.set_product(product.toStdString());
   ohlcRequest.set_interval(static_cast<Interval>(interval));
   ohlcRequest.set_count(requestLimit);
   ohlcRequest.set_lesser_then(-1);

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

   bool firstPortion = candlesticksChart_->data()->size() == 0;

   auto product = ui_->cboInstruments->currentText();
   auto interval = dateRange_.checkedId();

   if (product != QString::fromStdString(response.product()) || interval != response.interval())
      return;

   OhlcCandle lastCandle;

   maxPrice = 0.0;
   minPrice = -1.0;

   qreal maxVolume = 0.0;
   qreal maxTimestamp = -1.0;

   for (int i = response.candles_size() - 1; i >= 0; --i)
   {
      auto candle = response.candles(i);
      maxPrice = qMax(maxPrice, candle.high());
      minPrice = minPrice == -1.0 ? candle.low() : qMin(minPrice, candle.low());
      maxVolume = qMax(maxVolume, candle.volume());
      maxTimestamp = qMax(maxTimestamp, static_cast<qreal>(candle.timestamp()));

      bool isLast = (i == 0);

      if (isLast)
      {
         if (lastHigh != 0.0 && lastLow != 0.0 && lastClose != 0.0)
         {
            candle.set_high(lastHigh);
            candle.set_low(lastLow);
            candle.set_close(lastClose);
         }
         lastCandle = candle;
      }

      AddDataPoint(candle.open(), candle.high(), candle.low(), candle.close(), candle.timestamp(), candle.volume());
      qDebug("Added: %s, open: %f, high: %f, low: %f, close: %f, volume: %f"
         , QDateTime::fromMSecsSinceEpoch(candle.timestamp()).toUTC().toString(Qt::ISODateWithMs).toStdString().c_str()
         , candle.open()
         , candle.high()
         , candle.low()
         , candle.close()
         , candle.volume());

      lastHigh = candle.high();
      lastLow = candle.low();
      lastClose = candle.close();
   }

   qDebug("Min price: %f, Max price: %f, Max volume: %f", minPrice, maxPrice, maxVolume);

   currentMinPrice = minPrice;
   currentMaxPrice = maxPrice;

   auto margin = qMax(maxPrice - minPrice, 0.01) / 10;
   minPrice -= margin;
   maxPrice += margin;
   minPrice = qMax(minPrice, 0.0);

   newMaxPrice = maxPrice;
   newMinPrice = minPrice;
   volumeAxisRect_->axis(QCPAxis::atRight)->setRange(0, maxVolume);
   if (firstPortion) {
	   first_timestamp_in_db = response.first_stamp_in_db() / 1000;
		UpdatePlot(interval, maxTimestamp);
   }


}

void ChartWidget::setAutoScaleBtnColor() const
{
	if (autoScaling) {
		ui_->autoScaleBtn->setStyleSheet(QStringLiteral("background-color: transparent; border: none; color: rgb(36,124,172)"));
	}
	else {
		ui_->autoScaleBtn->setStyleSheet(QStringLiteral("background-color: transparent; border: none; color: rgb(255, 255, 255)"));
	}
}

void ChartWidget::AddNewCandle()
{
   const auto currentTimestamp = QDateTime::currentMSecsSinceEpoch();
   OhlcCandle candle;
   candle.set_open(lastClose);
   candle.set_close(lastClose);
   candle.set_high(lastClose);
   candle.set_low(lastClose);
   candle.set_timestamp(currentTimestamp);
   candle.set_volume(0.0);

   maxPrice = qMax(maxPrice, candle.high());
   minPrice = minPrice == -1.0 ? candle.low() : qMin(minPrice, candle.low());

   AddDataPoint(candle.open(), candle.high(), candle.low(), candle.close(), candle.timestamp(), candle.volume());
   qDebug("Added: %s, open: %f, high: %f, low: %f, close: %f, volume: %f"
      , QDateTime::fromMSecsSinceEpoch(candle.timestamp()).toUTC().toString(Qt::ISODateWithMs).toStdString().c_str()
      , candle.open()
      , candle.high()
      , candle.low()
      , candle.close()
      , candle.volume());

   UpdatePlot(dateRange_.checkedId(), currentTimestamp);
}

void ChartWidget::ModifyCandle()
{
   const auto& lastCandle = candlesticksChart_->data()->at(candlesticksChart_->data()->size() - 1);
   QCPFinancialData candle(*lastCandle);

   candle.close = lastClose;
   candle.high = lastHigh;
   candle.low = lastLow;

   candlesticksChart_->data()->remove(lastCandle->key);
   candlesticksChart_->data()->add(candle);
}

void ChartWidget::UpdatePlot(const int& interval, const qint64& timestamp)
{
   qreal size = IntervalWidth(interval, requestLimit);
   qreal upper = timestamp + 0.8 * IntervalWidth(interval) / 2;

   ui_->customPlot->rescaleAxes();
   ui_->customPlot->xAxis->setRange(upper / 1000, size / 1000, Qt::AlignRight);
   ui_->customPlot->yAxis2->setRange(minPrice, maxPrice);
   ui_->customPlot->yAxis2->setNumberPrecision(FractionSizeForProduct(title_->text()));
   ui_->customPlot->replot();

}

void ChartWidget::timerEvent(QTimerEvent* event)
{
   killTimer(timerId);

   timerId = startTimer(getTimerInterval());

   AddNewCandle();
}

std::chrono::seconds ChartWidget::getTimerInterval() const
{
   auto currentTime = QDateTime::fromMSecsSinceEpoch(static_cast<qint64> (currentTimestamp)).time();

   auto timerInterval = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::milliseconds ((int) IntervalWidth(dateRange_.checkedId())));
   if (currentTime.second() != std::chrono::seconds(0).count())
   {
      if (currentTime.minute() != std::chrono::minutes(0).count())
      {
         auto currentSeconds = currentTime.minute() * 60 + currentTime.second();
         auto diff = timerInterval.count() - currentSeconds;
         timerInterval = std::chrono::seconds(diff);
      }
   }

   qDebug() << "timer will start after " << timerInterval.count() << " seconds";

   return timerInterval;
}

std::pair<qint64, qint64> ChartWidget::GetPlotRange() const
{
	return { volumeAxisRect_->axis(QCPAxis::atBottom)->range().lower,
			 volumeAxisRect_->axis(QCPAxis::atBottom)->range().upper };
}

void ChartWidget::LoadAdditionalPoints(const QCPRange& range) const
{
	const auto data = candlesticksChart_->data();
	if (data->size() && (range.lower - data->constBegin()->key < IntervalWidth(dateRange_.checkedId()) / 1000 * loadDistance) && first_timestamp_in_db < data->constBegin()->key) {
		OhlcRequest ohlcRequest;
		auto product = ui_->cboInstruments->currentText();
		ohlcRequest.set_product(product.toStdString());
		ohlcRequest.set_interval(static_cast<Interval>(dateRange_.checkedId()));
		ohlcRequest.set_count(requestLimit);
		ohlcRequest.set_lesser_then(data->constBegin()->key * 1000);

		MarketDataHistoryRequest request;
		request.set_request_type(MarketDataHistoryMessageType::OhlcHistoryType);
		request.set_request(ohlcRequest.SerializeAsString());
		mdhsClient_->SendRequest(request);
	}
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
	if (lastInterval != interval)
	{
		lastInterval = interval;
		qDebug() << "clicked" << interval;
		UpdateChart(interval);
	}
}

void ChartWidget::OnInstrumentChanged(const QString &text) {
   if (title_ != nullptr)
   {
	   if (text != title_->text())
	   {
		   UpdateChart(dateRange_.checkedId());
	   }
   }
}

void ChartWidget::OnPlotMouseMove(QMouseEvent *event)
{
   if (info_ == nullptr)
      return;

   if (auto plottable = ui_->customPlot->plottableAt(event->localPos()))
   {
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
                     .arg(volumeValue.value, 0, 'g', -1));
	  ui_->customPlot->replot();
   } else 
   {
      info_->setText({});
	  ui_->customPlot->replot();
   }

   if (isDraggingYAxis)
   {
	   ui_->customPlot->axisRect()->setRangeDrag(ui_->customPlot->yAxis2->orientation());

	   bool minusY = event->y() < dragY;
	   dragY = event->y();

	   auto margin = qMax(newMaxPrice - newMinPrice, 0.01) / 10;
	   margin = minusY ? -margin : margin;
	   newMinPrice -= margin;
	   newMaxPrice += margin;
	   newMinPrice = qMax(newMinPrice, 0.0);

	   ui_->customPlot->yAxis2->setRange(newMinPrice, newMaxPrice);
	   ui_->customPlot->yAxis2->setNumberPrecision(FractionSizeForProduct(title_->text()));
	   ui_->customPlot->replot();
   }
   else
   {
	   ui_->customPlot->axisRect()->setRangeDrag(Qt::Horizontal | Qt::Vertical);
   }
   if (isDraggingXAxis) {
	   auto bottomAxis = volumeAxisRect_->axis(QCPAxis::atBottom);
	   auto currentXPos = event->pos().x();
	   auto lower_bound = volumeAxisRect_->axis(QCPAxis::atBottom)->range().lower;
	   auto upper_bound = volumeAxisRect_->axis(QCPAxis::atBottom)->range().upper;
	   auto diff = upper_bound - lower_bound;
	   auto directionCoeff = (currentXPos - lastDragCoordX > 0) ? -1 : 1;
	   double scalingCoeff = qAbs(currentXPos - startDragCoordX) / ui_->customPlot->size().width();
	   lastDragCoordX = currentXPos;
	   double tempCoeff = 4.0; //change this to impact on xAxis scale speed
	   lower_bound += diff / tempCoeff * scalingCoeff * directionCoeff;
	   upper_bound -= diff / tempCoeff * scalingCoeff * directionCoeff;
	   bottomAxis->setRange(lower_bound, upper_bound);
	   ui_->customPlot->replot();
   }
}

void ChartWidget::rescalePlot()
{
	auto lower_bound = volumeAxisRect_->axis(QCPAxis::atBottom)->range().lower;
	auto upper_bound = volumeAxisRect_->axis(QCPAxis::atBottom)->range().upper;
	currentMinPrice = std::numeric_limits<qreal>::max();
	currentMaxPrice = std::numeric_limits<qreal>::min();
	for (const auto& it : *candlesticksChart_->data()) {
		if (it.key >= lower_bound && it.key <= upper_bound) {
			currentMinPrice = qMin(currentMinPrice, it.low);
			currentMaxPrice = qMax(currentMaxPrice, it.high);
		}
	}
	ui_->customPlot->yAxis2->setRange(currentMinPrice, currentMaxPrice);
	ui_->customPlot->replot();
}

void ChartWidget::OnMousePressed(QMouseEvent* event)
{
	auto select = ui_->customPlot->yAxis2->selectTest(event->pos(), false);
	isDraggingYAxis = select != -1.0;
	if (isDraggingYAxis) {
		if (autoScaling) {
			ui_->autoScaleBtn->animateClick();
		}
	}

	auto selectXPoint = volumeAxisRect_->axis(QCPAxis::atBottom)->selectTest(event->pos(), false);
	isDraggingXAxis = selectXPoint != -1.0;
	if (isDraggingXAxis) {
		ui_->customPlot->setInteraction(QCP::iRangeDrag, false);
		volumeAxisRect_->axis(QCPAxis::atBottom)->axisRect()->setRangeDrag(volumeAxisRect_->axis(QCPAxis::atBottom)->orientation());
		lastDragCoordX = event->pos().x();
		startDragCoordX = event->pos().x();
	}

	if (ui_->customPlot->axisRect()->rect().contains(event->pos()) || volumeAxisRect_->rect().contains(event->pos())) {
		isDraggingMainPlot = true;
	}
}

void ChartWidget::OnMouseReleased(QMouseEvent* event)
{
	isDraggingYAxis = false;
	isDraggingXAxis = false;
	isDraggingMainPlot = false;
	ui_->customPlot->setInteraction(QCP::iRangeDrag, true);
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
   connect(volumeAxisRect_->axis(QCPAxis::atBottom),
	       qOverload<const QCPRange&>(&QCPAxis::rangeChanged),
	   this,
	   [this](QCPRange range){
				LoadAdditionalPoints(range);
			   rescalePlot();
				ui_->customPlot->xAxis->setRange(range);
			});
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
   ui_->customPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);

   connect(ui_->customPlot, &QCustomPlot::mouseMove, this, &ChartWidget::OnPlotMouseMove);
   connect(ui_->customPlot, &QCustomPlot::mousePress, this, &ChartWidget::OnMousePressed);
   connect(ui_->customPlot, &QCustomPlot::mouseRelease, this, &ChartWidget::OnMouseReleased);

   // make zoomable
}
