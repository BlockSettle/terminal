#ifndef CHARTWIDGET_H
#define CHARTWIDGET_H

#include <QWidget>
#include <QtCharts>
#include "CommonTypes.h"

namespace Ui {
class ChartWidget;
}

using namespace QtCharts;
class MarketDataProvider;
class ApplicationSettings;
class ArmoryConnection;

class ChartWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ChartWidget(QWidget *parent = nullptr);
    ~ChartWidget();
    void init(const std::shared_ptr<ApplicationSettings> &, const std::shared_ptr<MarketDataProvider> &
       , const std::shared_ptr<ArmoryConnection> &);

protected slots:
   void onDateRangeChanged(int id);
   void onMDUpdated(bs::network::Asset::Type, const QString &security, bs::network::MDFields);

protected:
   void setChartStyle();
   void createCandleChartAxis();
   void createVolumeChartAxis();
   void buildCandleChart();
   void addDataPoint(qreal open, qreal high, qreal low, qreal close, qreal timestamp, qreal volume);

private:
    Ui::ChartWidget *ui_;
    QButtonGroup dateRange_;
    std::shared_ptr<MarketDataProvider> mdProvider_;
    QStandardItemModel *cboModel_;
    QCandlestickSeries *priceSeries_;
    QCandlestickSeries *volumeSeries_;
};

#endif // CHARTWIDGET_H
