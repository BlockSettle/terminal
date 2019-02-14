#ifndef CHARTWIDGET_H
#define CHARTWIDGET_H

#include <QWidget>
#include <QButtonGroup>
#include "CommonTypes.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class ChartWidget;
}
namespace spdlog {
   class logger;
}
class QStandardItemModel;
class QCPTextElement;
class QCPFinancial;
class QCPBars;
class QCPAxisRect;
QT_END_NAMESPACE

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
   void onInstrumentChanged(const QString &text);
   void onPlotMouseMove(QMouseEvent* event);

protected:
   void initializeCustomPlot();
   void updateChart(int interval = -1);
   void addDataPoint(qreal open, qreal high, qreal low, qreal close, qreal timestamp, qreal volume);
   qreal intervalWidth(int interval = -1, int count = 1) const;
   int fractionSizeForProduct(const QString &product) const;

private:
    Ui::ChartWidget *ui_;
    QButtonGroup dateRange_;
    std::shared_ptr<MarketDataProvider> mdProvider_;
    QStandardItemModel *cboModel_;
    std::shared_ptr<TradesClient> client_;
    QCPTextElement *title_;
    QCPTextElement *info_;
    QCPFinancial *candlesticksChart_;
    QCPBars *volumeChart_;
    QCPAxisRect *volumeAxisRect_;
};

#endif // CHARTWIDGET_H
