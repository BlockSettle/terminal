/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "ChartWidget.h"
#include "spdlog/logger.h"
#include "ui_ChartWidget.h"
#include "Colors.h"
#include "MarketDataProvider.h"
#include "MdhsClient.h"
#include "market_data_history.pb.h"
#include "trade_history.pb.h"
#include "ApplicationSettings.h"

const QColor BACKGROUND_COLOR = QColor(28, 40, 53);
const QColor FOREGROUND_COLOR = QColor(Qt::white);
const QColor VOLUME_COLOR = QColor(32, 159, 223);

using namespace Blocksettle::Communication::TradeHistory;

ComboBoxDelegate::ComboBoxDelegate(QObject* parent)
   : QItemDelegate(parent)
{
}

void ComboBoxDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
   if (index.data(Qt::AccessibleDescriptionRole).toString() == QLatin1String("separator")) {
      painter->setPen(Qt::gray);
      painter->drawLine(option.rect.left(), option.rect.center().y(), option.rect.right(), option.rect.center().y());
   }
   else if (index.data(Qt::AccessibleDescriptionRole).toString() == QLatin1String("parent")) {
      QStyleOptionViewItem parentOption = option;
      parentOption.state |= QStyle::State_Enabled;
      QItemDelegate::paint(painter, parentOption, index);
   }
   else if (index.data(Qt::AccessibleDescriptionRole).toString() == QLatin1String("child")) {
      QStyleOptionViewItem childOption = option;
      int indent = option.fontMetrics.width(QString(4, QChar::fromLatin1(' ')));
      childOption.rect.adjust(indent, 0, 0, 0);
      childOption.textElideMode = Qt::ElideNone;
      QItemDelegate::paint(painter, childOption, index);
   }
   else {
      QItemDelegate::paint(painter, option, index);
   }
}

QSize ComboBoxDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
   QString type = index.data(Qt::AccessibleDescriptionRole).toString();
   if (type == QLatin1String("separator"))
      return QSize(0, 10);
   return QItemDelegate::sizeHint(option, index);
}

ChartWidget::ChartWidget(QWidget* pParent)
   : QWidget(pParent)
     , ui_(new Ui::ChartWidget)
     , candlesticksChart_(nullptr)
     , volumeChart_(nullptr)
     , volumeAxisRect_(nullptr)
     , lastHigh_(0.0)
     , lastLow_(0.0)
     , lastClose_(0.0)
     , currentTimestamp_(0)
     , lastInterval_(-1)
     , dragY_(0)
     , isDraggingYAxis_(false)
{
   ui_->setupUi(this);
   horLine = new QCPItemLine(ui_->customPlot);
   vertLine = new QCPItemLine(ui_->customPlot);
   setAutoScaleBtnColor();
   connect(ui_->autoScaleBtn, &QPushButton::clicked, this, &ChartWidget::OnAutoScaleBtnClick);

   setMouseTracking(true);
   connect(ui_->resetBtn, &QPushButton::clicked, this, &ChartWidget::OnResetBtnClick);
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


   cboModel_ = new QStandardItemModel(this);
   ui_->cboInstruments->setItemDelegate(new ComboBoxDelegate);
   ui_->cboInstruments->setModel(cboModel_);

   //uncomment when there will we enought data
   ui_->btn1y->hide();
   ui_->btn6m->hide();
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

   connect(ui_->pushButtonMDConnection, &QPushButton::clicked, this, &ChartWidget::ChangeMDSubscriptionState);

   connect(mdProvider.get(), &MarketDataProvider::MDUpdate, this, &ChartWidget::OnMdUpdated);
   connect(mdProvider.get(), &MarketDataProvider::OnNewFXTrade, this, &ChartWidget::OnNewXBTorFXTrade);
   connect(mdProvider.get(), &MarketDataProvider::OnNewPMTrade, this, &ChartWidget::OnNewPMTrade);
   connect(mdProvider.get(), &MarketDataProvider::OnNewXBTTrade, this, &ChartWidget::OnNewXBTorFXTrade);
   connect(mdProvider.get(), &MarketDataProvider::WaitingForConnectionDetails, this, &ChartWidget::OnLoadingNetworkSettings);
   connect(mdProvider.get(), &MarketDataProvider::StartConnecting, this, &ChartWidget::OnMDConnecting);
   connect(mdProvider.get(), &MarketDataProvider::Connected, this, &ChartWidget::OnMDConnected);
   connect(mdProvider.get(), &MarketDataProvider::Disconnecting, this, &ChartWidget::OnMDDisconnecting);
   connect(mdProvider.get(), &MarketDataProvider::Disconnected, this, &ChartWidget::OnMDDisconnected);

   // initialize charts
   InitializeCustomPlot();

   auto timeframe = appSettings_->get(ApplicationSettings::ChartTimeframe).toInt();
   const auto btns = dateRange_.buttons();
   for (auto it : btns) {
      if (dateRange_.id(it) == timeframe) {
         it->setChecked(true);
         break;
      }
   }
}

void ChartWidget::setAuthorized(bool authorized)
{
   ui_->pushButtonMDConnection->setEnabled(!authorized);
   authorized_ = authorized;
}

void ChartWidget::disconnect()
{
   OnMDDisconnecting();
}

ChartWidget::~ChartWidget()
{
   delete ui_;
}

void ChartWidget::SendEoDRequest()
{
   OhlcRequest ohlcRequest;
   ohlcRequest.set_product(getCurrentProductName().toStdString());
   ohlcRequest.set_interval(static_cast<Interval>(dateRange_.checkedId()));
   ohlcRequest.set_count(1);
   ohlcRequest.set_lesser_then(-1);

   MarketDataHistoryRequest request;
   request.set_request_type(MarketDataHistoryMessageType::EoDPriceType);
   request.set_request(ohlcRequest.SerializeAsString());
   mdhsClient_->SendRequest(request);
   eodRequestSent_ = true;
}

// Populate combo box with existing instruments comeing from mdProvider
void ChartWidget::OnMdUpdated(bs::network::Asset::Type assetType, const QString& security,
                              bs::network::MDFields mdFields)
{
   if ((assetType == bs::network::Asset::Undefined) && security.isEmpty()) // Celer disconnected
   {
      isProductListInitialized_ = false;
      cboModel_->clear();
      return;
   }
   if (!isProductListInitialized_) {
      isProductListInitialized_ = true;
      MarketDataHistoryRequest request;
      request.set_request_type(MarketDataHistoryMessageType::ProductsListType);
      mdhsClient_->SendRequest(request);
   }

   for (const auto& field : mdFields) {
      if (field.type == bs::network::MDField::MDTimestamp) {
         currentTimestamp_ = field.value;
         CheckToAddNewCandle(currentTimestamp_);
         auto date = QDateTime::fromMSecsSinceEpoch(currentTimestamp_, Qt::TimeSpec::UTC).time();
         if (!eodUpdated_
            && !eodRequestSent_
            && date.hour() == 0
            && date.minute() == 0
            && date.second() > 5
         ) {
            SendEoDRequest();
            QTimer::singleShot(5000, [this]()
            {
               if (!eodUpdated_) {
                  SendEoDRequest();
               }
            });
         }
         if (date.hour() != 0) {
            eodUpdated_ = false;
            eodRequestSent_ = false;
         }
      }
   }
}

void ChartWidget::UpdateChart(const int& interval)
{
   eodUpdated_ = false;
   eodRequestSent_ = false;
   auto product = getCurrentProductName();
   if (product.isEmpty())
      return;
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
   ohlcRequest.set_count(requestLimit);
   ohlcRequest.set_lesser_then(-1);

   MarketDataHistoryRequest request;
   request.set_request_type(MarketDataHistoryMessageType::OhlcHistoryType);
   request.set_request(ohlcRequest.SerializeAsString());
   mdhsClient_->SendRequest(request);
}

void ChartWidget::OnDataReceived(const std::string& data)
{
   if (data.empty()) {
      logger_->error("Empty data received from mdhs.");
      return;
   }

   MarketDataHistoryResponse response;
   if (!response.ParseFromString(data)) {
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
   case MarketDataHistoryMessageType::EoDPriceType:
      {
         ProcessEodResponse(response.response());
      }
      break;
   default:
      logger_->error("[ApiServerConnectionListener::OnDataReceived] undefined message type");
      break;
   }
}

void ChartWidget::ProcessProductsListResponse(const std::string& data)
{
   if (data.empty()) {
      logger_->error("Empty data received from mdhs.");
      return;
   }

   ProductsListResponse response;
   if (!response.ParseFromString(data)) {
      logger_->error("can't parse response from mdhs: {}", data);
      return;
   }
   std::map<TradeHistoryTradeType, std::vector<std::string>, std::greater<>> tempMap;
   for (const auto& product : response.products()) {
      tempMap[product.type()].push_back(product.product());
      productTypesMapper[product.product()] = product.type();
   }
   for (const auto& mapElement : tempMap) {
      AddParentItem(cboModel_, ProductTypeToString(mapElement.first));
      for (const auto& name : mapElement.second) {
         AddChildItem(cboModel_, QString::fromStdString(name));
      }
   }

   connect(ui_->cboInstruments, &QComboBox::currentTextChanged,
           this, &ChartWidget::OnInstrumentChanged);
   auto savedProduct = appSettings_->get(ApplicationSettings::ChartProduct).toString();
   bool found = false;
   for (int i = 0; i < ui_->cboInstruments->count(); i++) {
      if (ui_->cboInstruments->itemText(i).contains(savedProduct)) {
         ui_->cboInstruments->setCurrentIndex(i);
         found = true;
         break;
      }
   }
   if (!found) {
      ui_->cboInstruments->setCurrentIndex(1); //to prevent automatic selection of parent item
   }

   zoomDiff_ = 0.0;
}

void ChartWidget::ProcessOhlcHistoryResponse(const std::string& data)
{
   if (data.empty()) {
      logger_->error("Empty data received from mdhs.");
      return;
   }

   OhlcResponse response;
   if (!response.ParseFromString(data)) {
      logger_->error("can't parse response from mdhs: {}", data);
      return;
   }

   bool firstPortion = candlesticksChart_->data()->size() == 0;

   auto product = getCurrentProductName();
   auto interval = dateRange_.checkedId();

   if (product != QString::fromStdString(response.product()) || interval != response.interval())
      return;

   quint64 maxTimestamp = 0;

   for (int i = 0; i < response.candles_size(); i++) {
      auto candle = response.candles(i);
      maxTimestamp = qMax(maxTimestamp, static_cast<quint64>(candle.timestamp()));

      bool isLast = (i == 0);
      if (candle.timestamp() >= lastCandle_.timestamp() 
         || lastCandle_.timestamp() - candle.timestamp() < IntervalWidth( interval, 1, QDateTime::fromMSecsSinceEpoch(candle.timestamp(), Qt::TimeSpec::UTC))) {
         if (lastCandle_.timestamp() != 0) {
            logger_->error("Invalid distance between candles from mdhs. The last timestamp: {}  new timestamp: {}",
                           lastCandle_.timestamp(), candle.timestamp());
         }
      }
      else {
         if (lastCandle_.timestamp() - candle.timestamp() != IntervalWidth(
            interval, 1, QDateTime::fromMSecsSinceEpoch(candle.timestamp(), Qt::TimeSpec::UTC)) && candlesticksChart_->data()->size()) {
            for (int j = 0; j < (lastCandle_.timestamp() - candle.timestamp()) / IntervalWidth(
                    interval, 1, QDateTime::fromMSecsSinceEpoch(candle.timestamp(), Qt::TimeSpec::UTC)) - 1; j++) {
               AddDataPoint(candle.close(), candle.close(), candle.close(), candle.close(),
                            lastCandle_.timestamp() - IntervalWidth(interval) * (j + 1), 0);
            }
         }
      }

      lastCandle_ = candle;

      AddDataPoint(candle.open(), candle.high(), candle.low(), candle.close(), candle.timestamp(), candle.volume());
#if 0
      qDebug("Added: %s, open: %f, high: %f, low: %f, close: %f, volume: %f"
             , QDateTime::fromMSecsSinceEpoch(candle.timestamp(), Qt::TimeSpec::UTC)
               .toUTC().toString(Qt::ISODateWithMs).toStdString().c_str()
             , candle.open()
             , candle.high()
             , candle.low()
             , candle.close()
             , candle.volume());
#endif
      if (firstPortion && isLast) {
         lastHigh_ = candle.high();
         lastLow_ = candle.low();
         lastClose_ = candle.close();
      }
   }

   if (firstPortion) {
      if (!qFuzzyIsNull(currentTimestamp_)) {
         newestCandleTimestamp_ = GetCandleTimestamp(currentTimestamp_, static_cast<Interval>(interval));
      }
      else {
         logger_->warn("Data from mdhs came before MD update, or MD send wrong current timestamp");
         newestCandleTimestamp_ = GetCandleTimestamp(QDateTime::currentDateTimeUtc().toMSecsSinceEpoch(),
                                                     static_cast<Interval>(interval));
      }
      if (!response.candles_size()) {
         AddDataPoint(0, 0, 0, 0, newestCandleTimestamp_, 0);
         maxTimestamp = newestCandleTimestamp_;
      }
      else {
         if (newestCandleTimestamp_ > maxTimestamp) {
            auto lastCandle = *(candlesticksChart_->data()->at(candlesticksChart_->data()->size() - 1));
            for (quint64 i = 0; i < (newestCandleTimestamp_ - maxTimestamp) / IntervalWidth(interval); i++) {
               AddDataPoint(lastCandle.close, lastCandle.close, lastCandle.close, lastCandle.close,
                            newestCandleTimestamp_ - IntervalWidth(interval) * i, 0);
            }
            maxTimestamp = newestCandleTimestamp_;
         }
      }
      firstTimestampInDb_ = response.first_stamp_in_db() / 1000;
      UpdatePlot(interval, maxTimestamp);
   }
   else {
      LoadAdditionalPoints(volumeAxisRect_->axis(QCPAxis::atBottom)->range());
      rescalePlot();
      ui_->customPlot->replot();
   }
}

void ChartWidget::ProcessEodResponse(const std::string& data)
{
   eodRequestSent_ = false;
   EodPrice eodPrice;
   eodPrice.ParseFromString(data);
   if (getCurrentProductName().toStdString() != eodPrice.product()) {
      return;
   }
   if (candlesticksChart_->data()->size() < 2) {
      return;
   }
   auto delta = dateRange_.checkedId() >= Interval::TwentyFourHours ? 2 : 1; //should we update last or pre-last candle
   auto lastCandle = candlesticksChart_->data()->end() - delta;
   lastCandle->high = qMax(lastCandle->high, eodPrice.price());
   lastCandle->low = qMin(lastCandle->low, eodPrice.price());
   if (!qFuzzyCompare(lastCandle->close, eodPrice.price())) {
      lastCandle->close = eodPrice.price();
      UpdateOHLCInfo(IntervalWidth(dateRange_.checkedId()) / 1000,
                     ui_->customPlot->xAxis->pixelToCoord(ui_->customPlot->mapFromGlobal(QCursor::pos()).x()));
      rescalePlot();
      ui_->customPlot->replot();
   }
   eodUpdated_ = true;
}

double ChartWidget::CountOffsetFromRightBorder()
{
   return ui_->customPlot->xAxis->pixelToCoord(6) - ui_->customPlot->xAxis->pixelToCoord(0);
}

void ChartWidget::CheckToAddNewCandle(qint64 stamp)
{
   if (stamp <= newestCandleTimestamp_ + IntervalWidth(dateRange_.checkedId()) || !volumeChart_->data()->size()) {
      return;
   }
   auto candleStamp = GetCandleTimestamp(stamp, static_cast<Interval>(dateRange_.checkedId()));
   auto lastCandle = *(candlesticksChart_->data()->at(candlesticksChart_->data()->size() - 1));
   for (quint64 i = 0; i < (candleStamp - newestCandleTimestamp_) / IntervalWidth(dateRange_.checkedId()); i++) {
      AddDataPoint(lastCandle.close, lastCandle.close, lastCandle.close, lastCandle.close,
                   candleStamp - IntervalWidth(dateRange_.checkedId()) * i, 0);
   }
   newestCandleTimestamp_ = candleStamp;
   auto upper = ui_->customPlot->xAxis->range().upper;
   if (upper + IntervalWidth(dateRange_.checkedId()) / 1000 > newestCandleTimestamp_ / 1000) {
      ui_->customPlot->xAxis->moveRange(IntervalWidth(dateRange_.checkedId()) / 1000);
   }
   AddDataPoint(lastClose_, lastClose_, lastClose_, lastClose_, newestCandleTimestamp_, 0);
   ui_->customPlot->replot();
}

void ChartWidget::setAutoScaleBtnColor() const
{
   QString color = QStringLiteral("background-color: transparent; border: none; color: %1").
      arg(autoScaling_ ? QStringLiteral("rgb(36,124,172)") : QStringLiteral("rgb(255, 255, 255)"));
   ui_->autoScaleBtn->setStyleSheet(color);
}

void ChartWidget::DrawCrossfire(QMouseEvent* event)
{
   vertLine->start->setCoords(qMin(event->pos().x(), volumeAxisRect_->right() + 1), 0);
   vertLine->end->setCoords(qMin(event->pos().x(), volumeAxisRect_->right() + 1), volumeAxisRect_->bottom());
   horLine->start->setCoords(0, qMin(event->pos().y(), volumeAxisRect_->bottom()));
   horLine->end->setCoords(volumeAxisRect_->right(), qMin(event->pos().y(), volumeAxisRect_->bottom()));
   vertLine->setVisible(true);
   horLine->setVisible(true);
}

void ChartWidget::UpdatePrintFlag()
{
   if (candlesticksChart_->data()->isEmpty()) {
      lastPrintFlag_->setVisible(false);
      return;
   }
   lastPrintFlag_->setVisible(true);
   if (isHigh_) {
      lastPrintFlag_->setBrush(QBrush(c_greenColor));
   } else {
      lastPrintFlag_->setBrush(QBrush(c_redColor));
   }
   auto prec = FractionSizeForProduct(productTypesMapper[getCurrentProductName().toStdString()]);
   lastPrintFlag_->setText(QStringLiteral("-  ") + QString::number(lastClose_, 'f', prec));
   lastPrintFlag_->position->setCoords(ui_->customPlot->yAxis2->axisRect()->rect().right() + 2, ui_->customPlot->yAxis2->coordToPixel(lastClose_));
   ui_->customPlot->replot();
}

void ChartWidget::UpdatePlot(const int& interval, const qint64& timestamp)
{
   qreal upper = timestamp / 1000 + IntervalWidth(interval) / 1000 / 2;
   qreal lower = upper -
      IntervalWidth(dateRange_.checkedId(), appSettings_->get(ApplicationSettings::ChartCandleCount).toInt()) / 1000 -
      IntervalWidth(interval) / 1000 / 2;
   ui_->customPlot->xAxis->setRange(lower, upper);
   auto margin = IntervalWidth(dateRange_.checkedId()) / 1000 * 0.5;
   ui_->customPlot->xAxis->setRange(lower - margin, upper + margin);
   rescaleCandlesYAxis();
   ui_->customPlot->yAxis2->setNumberPrecision(
      FractionSizeForProduct(productTypesMapper[getCurrentProductName().toStdString()]));
   ui_->customPlot->replot();
   UpdatePrintFlag();
}

bool ChartWidget::needLoadNewData(const QCPRange& range, const QSharedPointer<QCPFinancialDataContainer> data) const
{
   return data->size() &&
      (range.lower - data->constBegin()->key < IntervalWidth(dateRange_.checkedId()) / 1000 * loadDistance)
      && firstTimestampInDb_ + IntervalWidth(OneHour) < data->constBegin()->key;
}

void ChartWidget::LoadAdditionalPoints(const QCPRange& range)
{
   const auto data = candlesticksChart_->data();
   if (needLoadNewData(range, data)) {
      if (qFuzzyCompare(prevRequestStamp, data->constBegin()->key)) {
         return;
      }
      OhlcRequest ohlcRequest;
      auto product = getCurrentProductName();
      ohlcRequest.set_product(product.toStdString());
      ohlcRequest.set_interval(static_cast<Interval>(dateRange_.checkedId()));
      ohlcRequest.set_count(requestLimit);
      ohlcRequest.set_lesser_then(data->constBegin()->key * 1000);

      prevRequestStamp = data->constBegin()->key;

      MarketDataHistoryRequest request;
      request.set_request_type(MarketDataHistoryMessageType::OhlcHistoryType);
      request.set_request(ohlcRequest.SerializeAsString());
      mdhsClient_->SendRequest(request);
   }
}

void ChartWidget::pickTicketDateFormat(const QCPRange& range) const
{
   const float rangeCoeff = 0.8;
   if (range.size() < 3 * 24 * 60 * 60 * rangeCoeff) {
      dateTimeTicker->setDateTimeFormat(QStringLiteral("dd MMM\nHH:mm"));
   }
   else if (range.size() < 365 * 24 * 60 * 60 * rangeCoeff) {
      dateTimeTicker->setDateTimeFormat(QStringLiteral("dd MMM\n"));
   }
   else {
      dateTimeTicker->setDateTimeFormat(QStringLiteral("MMM yyyy\n"));
   }
}

QString ChartWidget::getCurrentProductName() const
{
   return ui_->cboInstruments->currentText().simplified().replace(QStringLiteral(" "), QStringLiteral(""));
}

void ChartWidget::AddParentItem(QStandardItemModel* model, const QString& text)
{
   QStandardItem* item = new QStandardItem(text);
   item->setFlags(item->flags() & ~(Qt::ItemIsEnabled | Qt::ItemIsSelectable));
   item->setData(QStringLiteral("parent"), Qt::AccessibleDescriptionRole);
   QFont font = item->font();
   font.setBold(true);
   item->setFont(font);
   model->appendRow(item);
}

void ChartWidget::AddChildItem(QStandardItemModel* model, const QString& text)
{
   QStandardItem* item = new QStandardItem(text + QString(4, QChar::fromLatin1(' ')));
   item->setData(QStringLiteral("child"), Qt::AccessibleDescriptionRole);
   model->appendRow(item);
}

void ChartWidget::AddDataPoint(const qreal& open, const qreal& high, const qreal& low, const qreal& close,
                               const qreal& timestamp, const qreal& volume) const
{
   if (candlesticksChart_) {
      candlesticksChart_->data()->add(QCPFinancialData(timestamp / 1000, open, high, low, close));
   }
   if (volumeChart_) {
      volumeChart_->data()->add(QCPBarsData(timestamp / 1000, volume));
   }
}

quint64 ChartWidget::IntervalWidth(int interval, int count, const QDateTime& specialDate) const
{
   if (interval == -1) {
      return 1;
   }
   qreal hour = 3600000;
   switch (static_cast<Interval>(interval)) {
   case Interval::OneYear:
      return hour * (specialDate.isValid() ? specialDate.date().daysInYear() * 24 : 8760) * count;
   case Interval::SixMonths:
      return hour * (specialDate.isValid() ? 24 * specialDate.date().daysInMonth() * 6 : 4320) * count;
   case Interval::OneMonth:
      return hour * (specialDate.isValid() ? 24 * specialDate.date().daysInMonth() : 720) * count;
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

int ChartWidget::FractionSizeForProduct(TradeHistoryTradeType type)
{
   switch (type) {
   case FXTradeType:
      return 4;
   case XBTTradeType:
      return 2;
   case PMTradeType:
      return 6;
   default:
      return -1;
   }
}

// Handles changes of date range.
void ChartWidget::OnDateRangeChanged(int interval)
{
   if (lastInterval_ != interval) {
      appSettings_->set(ApplicationSettings::ChartTimeframe, interval);
      lastInterval_ = interval;
      zoomDiff_ = 0.0;
      UpdateChart(interval);
   }
}

void ChartWidget::OnInstrumentChanged(const QString& text)
{
   if (text != getCurrentProductName()) {
      appSettings_->set(ApplicationSettings::ChartProduct, getCurrentProductName());
      zoomDiff_ = volumeAxisRect_->axis(QCPAxis::atBottom)->range().size();
      isHigh_ = true;
      UpdateChart(dateRange_.checkedId());
   }
}

QString ChartWidget::GetFormattedStamp(double timestamp)
{
   QString resultFormat;
   switch (static_cast<Interval>(dateRange_.checkedId())) {
   case TwelveHours:
   case SixHours:
   case OneHour:
      resultFormat = QStringLiteral("dd MMM yy hh:mm");
      break;
   default:
      resultFormat = QStringLiteral("dd MMM yy");
   }
   return QDateTime::fromSecsSinceEpoch(qint64(timestamp)).toUTC().toString(resultFormat);
}

void ChartWidget::UpdateOHLCInfo(double width, double timestamp)
{
   auto ohlcValue = *candlesticksChart_->data()->findBegin(timestamp + width / 2);
   auto volumeValue = *volumeChart_->data()->findBegin(timestamp + width / 2);
   //ohlcValue.close >= ohlcValue.open ? c_greenColor : c_redColor
   const auto& color = VOLUME_COLOR.name();
   auto prec = FractionSizeForProduct(productTypesMapper[getCurrentProductName().toStdString()]);
   QString partForm = QStringLiteral("<font color=\"%2\">%1</font>");
   QString format =      QStringLiteral(
"&nbsp;&nbsp;%1&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;%2 %3&nbsp;&nbsp;&nbsp;%4 %5&nbsp;&nbsp;&nbsp;%6 %7&nbsp;&nbsp;&nbsp;%8 %9&nbsp;&nbsp;&nbsp;%10 %11"
)
                         .arg(partForm.arg(GetFormattedStamp(ohlcValue.key)).arg(FOREGROUND_COLOR.name()))
                         .arg(partForm.arg(QStringLiteral("O:")).arg(FOREGROUND_COLOR.name()))
                         .arg(partForm.arg(ohlcValue.open, 0, 'f', prec).arg(color))
                         .arg(partForm.arg(QStringLiteral("H:")).arg(FOREGROUND_COLOR.name()))
                         .arg(partForm.arg(ohlcValue.high, 0, 'f', prec).arg(color))
                         .arg(partForm.arg(QStringLiteral("L:")).arg(FOREGROUND_COLOR.name()))
                         .arg(partForm.arg(ohlcValue.low, 0, 'f', prec).arg(color))
                         .arg(partForm.arg(QStringLiteral("C:")).arg(FOREGROUND_COLOR.name()))
                         .arg(partForm.arg(ohlcValue.close, 0, 'f', prec).arg(color))
                         .arg(partForm.arg(QStringLiteral("Volume:")).arg(FOREGROUND_COLOR.name()))
                         .arg(partForm.arg(volumeValue.value).arg(color));
   ui_->ohlcLbl->setText(format);
}

void ChartWidget::OnPlotMouseMove(QMouseEvent* event)
{
   DrawCrossfire(event);

   double x = event->localPos().x();
   double width = IntervalWidth(dateRange_.checkedId()) / 1000;
   double timestamp = ui_->customPlot->xAxis->pixelToCoord(x);
   if (!candlesticksChart_->data()->size() ||
      timestamp > candlesticksChart_->data()->at(candlesticksChart_->data()->size() - 1)->key + width / 2 ||
      timestamp < candlesticksChart_->data()->at(0)->key - width / 2) {
      ui_->ohlcLbl->setText({});
   }
   else {
      UpdateOHLCInfo(width, timestamp);
   }

   if (isDraggingYAxis_) {
      auto rightAxis = ui_->customPlot->yAxis2;
      auto currentYPos = event->pos().y();
      auto lower_bound = rightAxis->range().lower;
      auto upper_bound = rightAxis->range().upper;
      auto diff = upper_bound - lower_bound;
      auto directionCoeff = (currentYPos - lastDragCoord_.y() > 0) ? -1 : 1;
      lastDragCoord_.setY(currentYPos);
      double tempCoeff = 28.0; //change this to impact on xAxis scale speed, the lower coeff the faster scaling
      upper_bound -= diff / tempCoeff * directionCoeff;
      lower_bound += diff / tempCoeff * directionCoeff;
      rightAxis->setRange(lower_bound, upper_bound);
   }
   if (isDraggingXAxis_) {
      auto bottomAxis = volumeAxisRect_->axis(QCPAxis::atBottom);
      auto currentXPos = event->pos().x();
      auto lower_bound = volumeAxisRect_->axis(QCPAxis::atBottom)->range().lower;
      auto upper_bound = volumeAxisRect_->axis(QCPAxis::atBottom)->range().upper;
      auto diff = upper_bound - lower_bound;
      auto directionCoeff = (currentXPos - lastDragCoord_.x() > 0) ? -1 : 1;
      //double scalingCoeff = qAbs(currentXPos - startDragCoordX_) / ui_->customPlot->size().width();
      lastDragCoord_.setX(currentXPos);
      double tempCoeff = 10.0; //change this to impact on xAxis scale speed, the lower coeff the faster scaling
      lower_bound += diff / tempCoeff * /*scalingCoeff * */ directionCoeff;
      auto lower_limit = candlesticksChart_->data()->constBegin()->key - (upper_bound - lower_bound) * 0.2;
      if (lower_bound < lower_limit && directionCoeff == -1) {
         return;
      }
      bottomAxis->setRange(lower_bound, upper_bound);
   }
   if (isDraggingMainPlot_) {
      auto axis = ui_->customPlot->xAxis;
      const double startPixel = dragStartPos_.x();
      const double currentPixel = event->pos().x();
      const double diff = axis->pixelToCoord(startPixel) - axis->pixelToCoord(currentPixel);
      auto size = candlesticksChart_->data()->size();
      double upper_bound = size ? candlesticksChart_->data()->at(size - 1)->key : QDateTime::currentSecsSinceEpoch();
      upper_bound += IntervalWidth(dateRange_.checkedId()) / 1000 / 2 + CountOffsetFromRightBorder();
      double lower_bound = QDateTime(QDate(2009, 1, 3)).toSecsSinceEpoch();
      if (dragStartRangeX_.upper + diff > upper_bound && diff > 0) {
         dragStartPos_.setX(event->pos().x());
         dragStartRangeX_ = axis->range();
      }
      else if (dragStartRangeX_.lower + diff < lower_bound && diff < 0) {
         dragStartPos_.setX(event->pos().x());
         dragStartRangeX_ = axis->range();
      }
      else {
         axis->setRange(dragStartRangeX_.lower + diff, dragStartRangeX_.upper + diff);
      }
      if (!autoScaling_) {
         auto axisY = ui_->customPlot->yAxis2;
         const double startPixelY = dragStartPos_.y();
         const double currentPixelY = event->pos().y();
         const double diffY = axisY->pixelToCoord(startPixelY) - axisY->pixelToCoord(currentPixelY);
         axisY->setRange(dragStartRangeY_.lower + diffY, dragStartRangeY_.upper + diffY);
      }

   }
   ui_->customPlot->replot();
}

void ChartWidget::leaveEvent(QEvent* event)
{
   vertLine->setVisible(false);
   horLine->setVisible(false);
   ui_->customPlot->replot();
}

void ChartWidget::rescaleCandlesYAxis()
{
   bool foundRange = false;
   auto keyRange = candlesticksChart_->keyAxis()->range();
   keyRange.upper += IntervalWidth(dateRange_.checkedId()) / 1000 / 2;
   keyRange.lower -= IntervalWidth(dateRange_.checkedId()) / 1000 / 2;
   auto newRange = candlesticksChart_->getValueRange(foundRange, QCP::sdBoth, keyRange);
   if (foundRange) {
      const double margin = 0.15;
      if (!QCPRange::validRange(newRange)) // likely due to range being zero
      {
         double center = (newRange.lower + newRange.upper) * 0.5;
         // upper and lower should be equal anyway, but just to make sure, incase validRange returned false for other reason
         newRange.lower = center - candlesticksChart_->valueAxis()->range().size() * margin / 2.0;
         newRange.upper = center + candlesticksChart_->valueAxis()->range().size() * margin / 2.0;
      }
      else {
         auto old = candlesticksChart_->valueAxis()->range();
         if (old != newRange) {
            newRange.lower -= newRange.size() * margin;
            newRange.upper += newRange.size() * margin;
         }
      }
      candlesticksChart_->valueAxis()->setRange(newRange);
   }
}

void ChartWidget::rescaleVolumesYAxis() const
{
   if (!volumeChart_->data()->size()) {
      return;
   }
   auto lower_bound = volumeAxisRect_->axis(QCPAxis::atBottom)->range().lower;
   auto upper_bound = volumeAxisRect_->axis(QCPAxis::atBottom)->range().upper;
   double maxVolume = volumeChart_->data()->constBegin()->value;
   for (const auto& it : *volumeChart_->data()) {
      if (it.key >= lower_bound && it.key <= upper_bound) {
         maxVolume = qMax(maxVolume, it.value);
      }
   }
   if (!qFuzzyCompare(maxVolume, volumeAxisRect_->axis(QCPAxis::atBottom)->range().upper)) {
      volumeAxisRect_->axis(QCPAxis::atRight)->setRange(0, maxVolume);
      ui_->customPlot->replot();
   }
}

void ChartWidget::rescalePlot()
{
   if (autoScaling_) {
      rescaleCandlesYAxis();
   }
   rescaleVolumesYAxis();
}

void ChartWidget::OnMousePressed(QMouseEvent* event)
{
   auto select = ui_->customPlot->yAxis2->selectTest(event->pos(), false);
   isDraggingYAxis_ = select != -1.0;
   if (isDraggingYAxis_) {
      if (autoScaling_) {
         ui_->autoScaleBtn->animateClick();
      }
   }

   auto selectXPoint = volumeAxisRect_->axis(QCPAxis::atBottom)->selectTest(event->pos(), false);
   isDraggingXAxis_ = selectXPoint != -1.0;
   if (isDraggingXAxis_) {
      volumeAxisRect_->axis(QCPAxis::atBottom)->axisRect()->setRangeDrag(
         volumeAxisRect_->axis(QCPAxis::atBottom)->orientation());
      startDragCoordX_ = event->pos().x();
   }

   if (ui_->customPlot->axisRect()->rect().contains(event->pos()) || volumeAxisRect_->rect().contains(event->pos())) {
      dragStartRangeX_ = ui_->customPlot->xAxis->range();
      dragStartRangeY_ = ui_->customPlot->yAxis2->range();
      dragStartPos_ = event->pos();
      isDraggingMainPlot_ = true;
   }

   if (isDraggingXAxis_ || isDraggingYAxis_) {
      lastDragCoord_ = event->pos();
      isDraggingMainPlot_ = false;
   }
}

void ChartWidget::OnMouseReleased(QMouseEvent* event)
{
   isDraggingYAxis_ = false;
   isDraggingXAxis_ = false;
   isDraggingMainPlot_ = false;
   //ui_->customPlot->setInteraction(QCP::iRangeDrag, true);
}

void ChartWidget::OnWheelScroll(QWheelEvent* event)
{
   auto bottomAxis = volumeAxisRect_->axis(QCPAxis::atBottom);
   auto lower_bound = volumeAxisRect_->axis(QCPAxis::atBottom)->range().lower;
   auto upper_bound = volumeAxisRect_->axis(QCPAxis::atBottom)->range().upper;
   auto diff = upper_bound - lower_bound;
   auto directionCoeff = event->angleDelta().y() < 0 ? -1 : 1;
   double tempCoeff = 120.0 / qAbs(event->angleDelta().y()) * 10;
   //change this to impact on xAxis scale speed, the lower coeff the faster scaling
   lower_bound += diff / tempCoeff * directionCoeff;
   auto lower_limit = candlesticksChart_->data()->constBegin()->key - (upper_bound - lower_bound) * 0.2 ;
   if (lower_bound < lower_limit && directionCoeff == -1) {
      return;
   }
   bottomAxis->setRange(lower_bound, upper_bound);
   ui_->customPlot->replot();
}

void ChartWidget::OnAutoScaleBtnClick()
{
   autoScaling_ = !autoScaling_;
   if (autoScaling_) {
      rescalePlot();
   }
   setAutoScaleBtnColor();
}

void ChartWidget::OnResetBtnClick()
{
   if (candlesticksChart_->data()->size()) {
      auto new_upper = candlesticksChart_->data()->at(candlesticksChart_->data()->size() - 1)->key + IntervalWidth(
         dateRange_.checkedId()) / 1000 / 2;
      QCPRange defaultRange(new_upper - IntervalWidth(dateRange_.checkedId(), requestLimit) / 1000, new_upper);
      volumeAxisRect_->axis(QCPAxis::atBottom)->setRange(defaultRange);
      volumeAxisRect_->axis(QCPAxis::atBottom)->setRange(defaultRange.lower - CountOffsetFromRightBorder(),
                                                         defaultRange.upper + CountOffsetFromRightBorder());
   }
   if (!autoScaling_) {
      autoScaling_ = true;
      rescalePlot();
      setAutoScaleBtnColor();
   }
}

void ChartWidget::resizeEvent(QResizeEvent* event)
{
   QWidget::resizeEvent(event);
   QMetaObject::invokeMethod(this, &ChartWidget::UpdatePrintFlag, Qt::QueuedConnection);//UpdatePrintFlag should be called after chart have resized, so we put this method to event loop's queue 
}

quint64 ChartWidget::GetCandleTimestamp(const uint64_t& timestamp, const Interval& interval) const
{
   QDateTime now = QDateTime::fromMSecsSinceEpoch(timestamp).toUTC();
   QDateTime result = now;
   switch (interval) {
   case Interval::OneYear:
      {
         result.setTime(QTime(0, 0));
         result.setDate(QDate(now.date().year(), 1, 1));
         break;
      }
   case Interval::SixMonths:
      {
         int month = now.date().month(); // 1 - January, 12 - December
         int mod = month % 6;
         result.setTime(QTime(0, 0));
         result.setDate(QDate(now.date().year(), month - mod + 1, 1));
         break;
      }
   case Interval::OneMonth:
      {
         result.setTime(QTime(0, 0));
         result.setDate(QDate(now.date().year(), now.date().month(), 1));
         break;
      }
   case Interval::OneWeek:
      {
         auto date = now.date();
         auto start = date.addDays(1 - date.dayOfWeek()); //1 - Monday, 7 - Sunday
         result.setTime(QTime(0, 0));
         result.setDate(start);
         break;
      }
   case Interval::TwentyFourHours:
      result.setTime(QTime(0, 0));
      break;
   case Interval::TwelveHours:
      {
         int hour = now.time().hour();
         int mod = hour % 12;
         result.setTime(QTime(hour - mod, 0));
         break;
      }
   case Interval::SixHours:
      {
         int hour = now.time().hour();
         int mod = hour % 6;
         result.setTime(QTime(hour - mod, 0));
         break;
      }
   case Interval::OneHour:
      result.setTime(QTime(now.time().hour(), 0));
      break;
   default:
      break;
   }
   return result.toMSecsSinceEpoch();
}

bool ChartWidget::isBeyondUpperLimit(QCPRange newRange, int interval)
{
   return (newRange.size() > IntervalWidth(interval, candleCountOnScreenLimit) / 1000) && !isDraggingMainPlot_;
}

bool ChartWidget::isBeyondLowerLimit(QCPRange newRange, int interval)
{
   return newRange.size() < IntervalWidth(interval, candleViewLimit) / 1000;
}

void ChartWidget::OnVolumeAxisRangeChanged(QCPRange newRange, QCPRange oldRange)
{
   auto interval = dateRange_.checkedId() == -1 ? 0 : dateRange_.checkedId();

   if (isBeyondUpperLimit(newRange, interval) && oldRange.lower >= 0) {
      volumeAxisRect_->axis(QCPAxis::atBottom)->setRange(
         oldRange.upper - IntervalWidth(interval, candleCountOnScreenLimit) / 1000, oldRange.upper);
      ui_->customPlot->xAxis->setRange(volumeAxisRect_->axis(QCPAxis::atBottom)->range());
   }
   else {
      if (isBeyondLowerLimit(newRange, interval)) {
         volumeAxisRect_->axis(QCPAxis::atBottom)->setRange(
            oldRange.upper - IntervalWidth(interval, candleViewLimit) / 1000 - 1.0, oldRange.upper);
         ui_->customPlot->xAxis->setRange(volumeAxisRect_->axis(QCPAxis::atBottom)->range());
      }
      else {
         ui_->customPlot->xAxis->setRange(newRange);
      }
   }
   if (!std::isinf(ui_->customPlot->xAxis->range().size() / (IntervalWidth(dateRange_.checkedId()) / 1000))) {
      appSettings_->set(ApplicationSettings::ChartCandleCount, int(ui_->customPlot->xAxis->range().size() / (IntervalWidth(dateRange_.checkedId()) / 1000)));
   }
   LoadAdditionalPoints(newRange);
   pickTicketDateFormat(newRange);
   rescalePlot();
}

QString ChartWidget::ProductTypeToString(TradeHistoryTradeType type)
{
   switch (type) {
   case FXTradeType: return QStringLiteral("FX");
   case XBTTradeType: return QStringLiteral("XBT");
   case PMTradeType: return QStringLiteral("PM");
   default: return QStringLiteral("");
   }
}

void ChartWidget::SetupCrossfire()
{
   QPen pen(Qt::white, 1, Qt::PenStyle::DashLine);
   QVector<qreal> dashes;
   qreal space = 8;
   dashes << 4 << space;
   pen.setDashPattern(dashes);

   vertLine->setLayer(QStringLiteral("axes"));
   horLine->setLayer(QStringLiteral("axes"));

   vertLine->start->setType(QCPItemPosition::ptAbsolute);
   vertLine->end->setType(QCPItemPosition::ptAbsolute);
   vertLine->setPen(pen);
   vertLine->setClipToAxisRect(false);

   horLine->start->setType(QCPItemPosition::ptAbsolute);
   horLine->end->setType(QCPItemPosition::ptAbsolute);
   horLine->setPen(pen);
   horLine->setClipToAxisRect(false);

   if (!ui_->customPlot->rect().contains(mapFromGlobal(QCursor::pos()))) {
      vertLine->setVisible(false);
      horLine->setVisible(false);
   }
}

void ChartWidget::SetupLastPrintFlag()
{
   lastPrintFlag_ = new QCPItemText(ui_->customPlot);
   lastPrintFlag_->setVisible(false);
   lastPrintFlag_->setPen(Qt::NoPen);
   lastPrintFlag_->setColor(Qt::white);
   lastPrintFlag_->setBrush(QBrush(c_greenColor));
   auto font = ui_->customPlot->axisRect()->axis(QCPAxis::atRight)->labelFont();
   lastPrintFlag_->setFont(font);
   lastPrintFlag_->position->setType(QCPItemPosition::ptAbsolute);
   lastPrintFlag_->position->setAxisRect(ui_->customPlot->yAxis2->axisRect());
   lastPrintFlag_->setPositionAlignment(Qt::AlignLeft | Qt::AlignVCenter);
   lastPrintFlag_->setLayer(QStringLiteral("axes"));
   lastPrintFlag_->setClipAxisRect(ui_->customPlot->yAxis2->axisRect());
   lastPrintFlag_->setClipToAxisRect(false);
}

void ChartWidget::InitializeCustomPlot()
{
   SetupCrossfire();

   QBrush bgBrush(BACKGROUND_COLOR);
   ui_->customPlot->setBackground(bgBrush);

   ui_->ohlcLbl->setFont(QFont(QStringLiteral("sans"), 10));

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
   ui_->customPlot->plotLayout()->addElement(1, 0, volumeAxisRect_);
   volumeAxisRect_->setMaximumSize(QSize(QWIDGETSIZE_MAX, 100));
   volumeAxisRect_->axis(QCPAxis::atBottom)->setLayer(QStringLiteral("axes"));
   volumeAxisRect_->axis(QCPAxis::atBottom)->grid()->setLayer(QStringLiteral("grid"));
   // bring bottom and main axis rect closer together:
   ui_->customPlot->plotLayout()->setRowSpacing(0);
   volumeAxisRect_->setAutoMargins(QCP::msLeft | QCP::msRight | QCP::msBottom);
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
   volumeAxisRect_->axis(QCPAxis::atRight)->ticker()->setTickCount(2);
   volumeAxisRect_->axis(QCPAxis::atRight)->setTickLabelFont(
      ui_->customPlot->axisRect()->axis(QCPAxis::atRight)->labelFont());

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
           qOverload<const QCPRange&, const QCPRange&>(&QCPAxis::rangeChanged),
           this,
           &ChartWidget::OnVolumeAxisRangeChanged);

   SetupLastPrintFlag();

   connect(ui_->customPlot->yAxis2,
      qOverload<const QCPRange&, const QCPRange&>(&QCPAxis::rangeChanged),
      this,
      [this]() {UpdatePrintFlag(); });

   // configure axes of both main and bottom axis rect:
   dateTimeTicker->setDateTimeSpec(Qt::UTC);
   dateTimeTicker->setDateTimeFormat(QStringLiteral("dd/MM/yy\nHH:mm"));
   dateTimeTicker->setTickCount(17);
   volumeAxisRect_->axis(QCPAxis::atBottom)->setTicker(dateTimeTicker);
   //volumeAxisRect_->axis(QCPAxis::atBottom)->setTickLabelRotation(10);
   volumeAxisRect_->axis(QCPAxis::atBottom)->setTickLabelFont(QFont(QStringLiteral("Arial"), 9));
   ui_->customPlot->xAxis->setBasePen(Qt::NoPen);
   ui_->customPlot->xAxis->setTickLabels(false);
   ui_->customPlot->xAxis->setTicks(false);
   // only want vertical grid in main axis rect, so hide xAxis backbone, ticks, and labels
   ui_->customPlot->xAxis->setTicker(dateTimeTicker);
   ui_->customPlot->rescaleAxes();
   ui_->customPlot->xAxis->scaleRange(1.025, ui_->customPlot->xAxis->range().center());
   ui_->customPlot->yAxis->scaleRange(1.1, ui_->customPlot->yAxis->range().center());

   // make axis rects' left side line up:
   QCPMarginGroup* group = new QCPMarginGroup(ui_->customPlot);
   ui_->customPlot->axisRect()->setMarginGroup(QCP::msLeft | QCP::msRight, group);
   volumeAxisRect_->setMarginGroup(QCP::msLeft | QCP::msRight, group);

   connect(ui_->customPlot, &QCustomPlot::mouseMove, this, &ChartWidget::OnPlotMouseMove);
   connect(ui_->customPlot, &QCustomPlot::mousePress, this, &ChartWidget::OnMousePressed);
   connect(ui_->customPlot, &QCustomPlot::mouseRelease, this, &ChartWidget::OnMouseReleased);
   connect(ui_->customPlot, &QCustomPlot::mouseWheel, this, &ChartWidget::OnWheelScroll);
   volumeAxisRect_->axis(QCPAxis::atRight)->setRange(0, 1000);
}

void ChartWidget::OnLoadingNetworkSettings()
{
   ui_->pushButtonMDConnection->setText(tr("Connecting"));
   ui_->pushButtonMDConnection->setEnabled(false);
   ui_->pushButtonMDConnection->setToolTip(tr("Waiting for connection details"));
}

void ChartWidget::OnMDConnecting()
{
   ui_->pushButtonMDConnection->setText(tr("Connecting"));
   ui_->pushButtonMDConnection->setEnabled(false);
   ui_->pushButtonMDConnection->setToolTip(QString{});
}

void ChartWidget::OnMDConnected()
{
   ui_->pushButtonMDConnection->setText(tr("Disconnect"));
   ui_->pushButtonMDConnection->setEnabled(!authorized_);
}

void ChartWidget::OnMDDisconnecting()
{
   ui_->pushButtonMDConnection->setText(tr("Disconnecting"));
   ui_->pushButtonMDConnection->setEnabled(false);

   if (candlesticksChart_ != nullptr)
      candlesticksChart_->data()->clear();

   if (volumeChart_ != nullptr)
      volumeChart_->data()->clear();

   ui_->ohlcLbl->setText({});
   ui_->customPlot->replot();

   mdProvider_->UnsubscribeFromMD();
   mdProvider_->DisconnectFromMDSource();
}

void ChartWidget::OnMDDisconnected()
{
   ui_->pushButtonMDConnection->setText(tr("Subscribe"));
   ui_->pushButtonMDConnection->setEnabled(!authorized_);
}

void ChartWidget::ChangeMDSubscriptionState()
{
   if (mdProvider_->IsConnectionActive()) {
      mdProvider_->DisconnectFromMDSource();
   }
   else {
      mdProvider_->SubscribeToMD();
   }
}

void ChartWidget::OnNewTrade(const std::string& productName, uint64_t timestamp, double price, double amount)
{
   if (productName != getCurrentProductName().toStdString() ||
      !candlesticksChart_->data()->size() ||
      !volumeChart_->data()->size()) {
      return;
   }

   auto lastVolume = volumeChart_->data()->end() - 1;
   lastVolume->value += amount;
   auto lastCandle = candlesticksChart_->data()->end() - 1;
   lastCandle->high = qMax(lastCandle->high, price);
   lastCandle->low = qMin(lastCandle->low, price);
   if (!qFuzzyCompare(lastCandle->close, price) || !qFuzzyIsNull(amount)) {
      isHigh_ = price > lastClose_;
      lastClose_ = price;
      UpdatePrintFlag();
      lastCandle->close = price;
      UpdateOHLCInfo(IntervalWidth(dateRange_.checkedId()) / 1000,
                     ui_->customPlot->xAxis->pixelToCoord(ui_->customPlot->mapFromGlobal(QCursor::pos()).x()));
      rescalePlot();
      ui_->customPlot->replot();
   }
   CheckToAddNewCandle(timestamp);
}

void ChartWidget::OnNewXBTorFXTrade(const bs::network::NewTrade& trade)
{
   OnNewTrade(trade.product, trade.timestamp, trade.price, trade.amount);
}

void ChartWidget::OnNewPMTrade(const bs::network::NewPMTrade& trade)
{
   OnNewTrade(trade.product, trade.timestamp, trade.price, trade.amount);
}
