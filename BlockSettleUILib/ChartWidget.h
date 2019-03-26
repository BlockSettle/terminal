#ifndef CHARTWIDGET_H
#define CHARTWIDGET_H

#include <QWidget>
#include <QButtonGroup>
#include "CommonTypes.h"
#include "qcustomplot.h"
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

using namespace Blocksettle::Communication::TradeHistory;

class ChartWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ChartWidget(QWidget* pParent = nullptr);
    ~ChartWidget();
    void init(const std::shared_ptr<ApplicationSettings>&
              , const std::shared_ptr<MarketDataProvider>&
              , const std::shared_ptr<ConnectionManager>&
              , const std::shared_ptr<spdlog::logger>&);

protected slots:
   void OnDataReceived(const std::string& data);
   void OnDateRangeChanged(int id);
   void OnMdUpdated(bs::network::Asset::Type, const QString &security, bs::network::MDFields);
   void OnInstrumentChanged(const QString &text);
   void OnPlotMouseMove(QMouseEvent* event);
   void rescaleCandlesYAxis();
   void rescaleVolumesYAxis() const;
   void rescalePlot();
   void OnMousePressed(QMouseEvent* event);
   void OnMouseReleased(QMouseEvent* event);
   void OnAutoScaleBtnClick();
   void OnResetBtnClick();
   bool isBeyondUpperLimit(QCPRange newRange, int interval);
   bool isBeyondLowerLimit(QCPRange newRange, int interval);
   void OnVolumeAxisRangeChanged(QCPRange newRange, QCPRange oneRange);

protected:
   void AddDataPoint(const qreal& open, const qreal& high, const qreal& low, const qreal& close, const qreal& timestamp, const qreal& volume) const;
   void UpdateChart(const int& interval) const;
   void InitializeCustomPlot();
   qreal IntervalWidth(int interval = -1, int count = 1) const;
   static int FractionSizeForProduct(TradeHistoryTradeType type);
   void ProcessProductsListResponse(const std::string& data);
   void ProcessOhlcHistoryResponse(const std::string& data);

   void setAutoScaleBtnColor() const;

   void AddNewCandle();
   void ModifyCandle();
   void UpdatePlot(const int& interval, const qint64& timestamp);

   void timerEvent(QTimerEvent* event);
   std::chrono::seconds getTimerInterval() const;
   bool needLoadNewData(const QCPRange& range, QSharedPointer<QCPFinancialDataContainer> data) const;

   void LoadAdditionalPoints(const QCPRange& range);

   void pickTicketDateFormat(const QCPRange& range) const;

private:
   std::shared_ptr<ApplicationSettings>			appSettings_;
   std::shared_ptr<MarketDataProvider>				mdProvider_;
   std::shared_ptr<MdhsClient>						mdhsClient_;
   std::shared_ptr<spdlog::logger>					logger_;

   std::map<std::string, TradeHistoryTradeType> productTypesMapper;

   QSharedPointer<QCPAxisTickerDateTime> dateTimeTicker{ new QCPAxisTickerDateTime };

   const int loadDistance{ 15 };

   constexpr static int requestLimit{ 100 };
   constexpr static int candleViewLimit{ 30 };
   constexpr static qint64 candleCountOnScreenLimit{ 1500 };

   Blocksettle::Communication::MarketDataHistory::OhlcCandle lastCandle_;

   double prevRequestStamp{ 0.0 };

   Ui::ChartWidget *ui_;
   QButtonGroup dateRange_;
   QStandardItemModel *cboModel_;
   QCPTextElement *title_;
   QCPTextElement *info_;
   QCPFinancial *candlesticksChart_;
   QCPBars *volumeChart_;
   QCPAxisRect *volumeAxisRect_;

   double lastHigh_;
   double lastLow_;
   double lastClose_;
   double currentTimestamp_;

   bool autoScaling_{ true };

   qreal currentMinPrice_{ 0 };
   qreal currentMaxPrice_{ 0 };

   int timerId_;
   int lastInterval_;
   int dragY_;

   bool isDraggingYAxis_;
   bool isDraggingXAxis_{ false };
   bool isDraggingMainPlot_{ false };

   QPoint lastDragCoord_;
   qreal startDragCoordX_{ 0.0 };

   quint64 firstTimestampInDb_{ 0 };
};

#endif // CHARTWIDGET_H
