/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef CHARTWIDGET_H
#define CHARTWIDGET_H

#include <QWidget>
#include <QButtonGroup>
#include "CommonTypes.h"
#include "CustomControls/qcustomplot.h"
#include "market_data_history.pb.h"

QT_BEGIN_NAMESPACE
namespace Ui { class ChartWidget; }
class QStandardItemModel;
class QCPTextElement;
class QCPFinancial;
class QCPBars;
class QCPAxisRect;
QT_END_NAMESPACE

class ApplicationSettings;
class MarketDataProvider;
class ConnectionManager;
namespace spdlog { class logger; }
class MdhsClient;

#include <QItemDelegate>
#include <QPainter>

class ComboBoxDelegate : public QItemDelegate
{
   Q_OBJECT
public:
   explicit ComboBoxDelegate(QObject *parent = nullptr);
protected:
   void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;
   QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const;
};

class ChartWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ChartWidget(QWidget* pParent = nullptr);
    ~ChartWidget() override;
    void SendEoDRequest();
    void init(const std::shared_ptr<ApplicationSettings>&
              , const std::shared_ptr<MarketDataProvider>&
              , const std::shared_ptr<ConnectionManager>&
              , const std::shared_ptr<spdlog::logger>&);

    void setAuthorized(bool authorized);
    void disconnect();

protected slots:
   void OnDataReceived(const std::string& data);
   void OnDateRangeChanged(int id);
   void OnMdUpdated(bs::network::Asset::Type, const QString &security, bs::network::MDFields);
   void OnInstrumentChanged(const QString &text);
   QString GetFormattedStamp(double timestamp);
   void UpdateOHLCInfo(double width, double timestamp);
   void OnPlotMouseMove(QMouseEvent* event);
   void leaveEvent(QEvent* event) override;
   void rescaleCandlesYAxis();
   void rescaleVolumesYAxis() const;
   void rescalePlot();
   void OnMousePressed(QMouseEvent* event);
   void OnMouseReleased(QMouseEvent* event);
   void OnWheelScroll(QWheelEvent* event);
   void OnAutoScaleBtnClick();
   void OnResetBtnClick();
   void resizeEvent(QResizeEvent* event) override;
   bool isBeyondUpperLimit(QCPRange newRange, int interval);
   bool isBeyondLowerLimit(QCPRange newRange, int interval);
   void OnVolumeAxisRangeChanged(QCPRange newRange, QCPRange oldRange);
   static QString ProductTypeToString(Blocksettle::Communication::TradeHistory::TradeHistoryTradeType type);
   void SetupCrossfire();
   void SetupLastPrintFlag();

   void OnLoadingNetworkSettings();
   void OnMDConnecting();
   void OnMDConnected();
   void OnMDDisconnecting();
   void OnMDDisconnected();
   void ChangeMDSubscriptionState();

   void OnNewTrade(const std::string& productName, uint64_t timestamp, double price, double amount);
   void OnNewXBTorFXTrade(const bs::network::NewTrade& trade);
   void OnNewPMTrade(const bs::network::NewPMTrade& trade);

protected:
   quint64 GetCandleTimestamp(const uint64_t& timestamp,
      const Blocksettle::Communication::MarketDataHistory::Interval& interval) const;
   void AddDataPoint(const qreal& open, const qreal& high, const qreal& low, const qreal& close, const qreal& timestamp, const qreal& volume) const;
   void UpdateChart(const int& interval);
   void InitializeCustomPlot();
   quint64 IntervalWidth(int interval = -1, int count = 1, const QDateTime& specialDate = {}) const;
   static int FractionSizeForProduct(Blocksettle::Communication::TradeHistory::TradeHistoryTradeType type);
   void ProcessProductsListResponse(const std::string& data);
   void ProcessOhlcHistoryResponse(const std::string& data);
   void ProcessEodResponse(const std::string& data);
   double CountOffsetFromRightBorder();

   void CheckToAddNewCandle(qint64 stamp);

   void setAutoScaleBtnColor() const;

   void DrawCrossfire(QMouseEvent* event);

   void UpdatePrintFlag();

   void UpdatePlot(const int& interval, const qint64& timestamp);

   bool needLoadNewData(const QCPRange& range, QSharedPointer<QCPFinancialDataContainer> data) const;

   void LoadAdditionalPoints(const QCPRange& range);

   void pickTicketDateFormat(const QCPRange& range) const;
private:
   QString getCurrentProductName() const;
   void AddParentItem(QStandardItemModel * model, const QString& text);
   void AddChildItem(QStandardItemModel* model, const QString& text);

private:
   std::shared_ptr<ApplicationSettings>			appSettings_;
   std::shared_ptr<MarketDataProvider>				mdProvider_;
   std::shared_ptr<MdhsClient>						mdhsClient_;
   std::shared_ptr<spdlog::logger>					logger_;

   bool                                         isProductListInitialized_{ false };
   std::map<std::string, Blocksettle::Communication::TradeHistory::TradeHistoryTradeType> productTypesMapper;

   QSharedPointer<QCPAxisTickerDateTime> dateTimeTicker{ new QCPAxisTickerDateTime };

   const int loadDistance{ 15 };

   bool eodUpdated_{ false };
   bool eodRequestSent_{ false };

   constexpr static int requestLimit{ 200 };
   constexpr static int candleViewLimit{ 150 };
   constexpr static qint64 candleCountOnScreenLimit{ 1500 };

   Blocksettle::Communication::MarketDataHistory::OhlcCandle lastCandle_;

   double prevRequestStamp{ 0.0 };

   double zoomDiff_{ 0.0 };

   Ui::ChartWidget *ui_;
   QButtonGroup dateRange_;
   QStandardItemModel *cboModel_;
   QCPFinancial *candlesticksChart_;
   QCPBars *volumeChart_;
   QCPAxisRect *volumeAxisRect_;

   QCPItemText *   lastPrintFlag_{ nullptr };
   bool isHigh_ { true };

   QCPItemLine* horLine;
   QCPItemLine* vertLine;

   double lastHigh_;
   double lastLow_;
   double lastClose_;
   double currentTimestamp_;
   quint64 newestCandleTimestamp_;

   bool autoScaling_{ true };

   qreal currentMinPrice_{ 0 };
   qreal currentMaxPrice_{ 0 };

   int lastInterval_;
   int dragY_;

   bool isDraggingYAxis_;
   bool isDraggingXAxis_{ false };
   bool isDraggingMainPlot_{ false };
   QCPRange dragStartRangeX_;
   QCPRange dragStartRangeY_;
   QPointF dragStartPos_;

   QPoint lastDragCoord_;
   qreal startDragCoordX_{ 0.0 };

   quint64 firstTimestampInDb_{ 0 };
   bool authorized_{ false };
};

#endif // CHARTWIDGET_H
