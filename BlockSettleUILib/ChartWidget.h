#ifndef CHARTWIDGET_H
#define CHARTWIDGET_H

#include <QWidget>
#include <QtCharts>
#include "CommonTypes.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class ChartWidget;
}
namespace spdlog {
   class logger;
}
class QCPTextElement;
class QCPFinancial;
class QCPBars;
class QCPAxisRect;
QT_END_NAMESPACE

using namespace QtCharts;
class MarketDataProvider;
class ApplicationSettings;
class ArmoryConnection;
class TradesClient;

class ChartWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ChartWidget(QWidget *parent = nullptr);
    ~ChartWidget();
    void init(const std::shared_ptr<ApplicationSettings> &
              , const std::shared_ptr<MarketDataProvider> &
              , const std::shared_ptr<ArmoryConnection> &
              , const std::shared_ptr<spdlog::logger>&);

protected slots:
   void onDateRangeChanged(int id);
   void onMDUpdated(bs::network::Asset::Type, const QString &security, bs::network::MDFields);
   void onPriceHover(bool status, QCandlestickSet *set);
   void onInstrumentChanged(const QString &text);

protected:
   void initializeCustomPlot();
   void setChartStyle();
   void createCandleChartAxis();
   void createVolumeChartAxis();
   void setupTimeAxis(QBarCategoryAxis *axis, bool labelsVisible, int interval = -1);
   QValueAxis *createValueAxis(QCandlestickSeries *series
                               , const QString &labelFormat
                               , qreal maxValue = -1.0
                               , qreal minValue = -1.0);
   void updatePriceValueAxis(const QString &labelFormat
                             , qreal maxValue = -1.0
                             , qreal minValue = -1.0);
   void updateVolumeValueAxis(const QString &labelFormat
                              , qreal maxValue = -1.0
                              , qreal minValue = -1.0);
   void buildCandleChart(int interval = -1);
   void updateChart(int interval = -1);
   void addDataPoint(qreal open, qreal high, qreal low, qreal close, qreal timestamp, qreal volume);
   qreal getZoomFactor(int interval = -1) const;
   qreal getPlotScale(int interval = -1) const;
   void setZoomFactor(qreal factor);
   QString barLabel(qreal timestamp, int interval = -1) const;
   qreal intervalLength(int interval = -1) const;

private:
    Ui::ChartWidget *ui_;
    QButtonGroup dateRange_;
    std::shared_ptr<MarketDataProvider> mdProvider_;
    QStandardItemModel *cboModel_;
    QCandlestickSeries *priceSeries_;
    QCandlestickSeries *volumeSeries_;
    QGraphicsTextItem *dataItemText_;
    std::shared_ptr<TradesClient> client_;
    QValueAxis *priceYAxis_;
    QValueAxis *volumeYAxis_;

    QCPTextElement *title;
    QCPFinancial *candlesticksChart;
    QCPBars *volumeChart;
    QCPAxisRect *volumeAxisRect;
};

#endif // CHARTWIDGET_H
