#ifndef CHARTWIDGET_H
#define CHARTWIDGET_H

#include <QWidget>
#include <QButtonGroup>
#include "CommonTypes.h"
#include "ApplicationSettings.h"
#include "ConnectionManager.h"
#include "RequestReplyCommand.h"

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
              , const std::shared_ptr<ConnectionManager> &
              , const std::shared_ptr<spdlog::logger>&);

signals:
	void AddDataPoint(qreal open, qreal high, qreal low, qreal close, qreal timestamp, qreal volume);
signals:
	void UpdateChart(int interval, QString product, qreal maxPrice, qreal minPrice, qreal maxVolume, qreal maxTimestamp);

protected slots:
	void OnAddDataPoint(qreal open, qreal high, qreal low, qreal close, qreal timestamp, qreal volume);
	void OnUpdateChart(int interval, QString product, qreal maxPrice, qreal minPrice, qreal maxVolume, qreal maxTimestamp);
	void onDateRangeChanged(int id);
	void onMDUpdated(bs::network::Asset::Type, const QString &security, bs::network::MDFields);
	void onInstrumentChanged(const QString &text);
	void onPlotMouseMove(QMouseEvent* event);

protected:
	void OnDataReceived(const std::string& data, const int& interval, const QString& product);
	void initializeCustomPlot();
	qreal intervalWidth(int interval = -1, int count = 1) const;
	int fractionSizeForProduct(const QString &product) const;

private:
	std::shared_ptr<ApplicationSettings>			appSettings_;
	std::shared_ptr<MarketDataProvider>				mdProvider_;
	std::shared_ptr<ConnectionManager>				connectionManager_;
	std::shared_ptr<spdlog::logger>					logger_;
	std::shared_ptr<RequestReplyCommand>			command_;

	Ui::ChartWidget *ui_;
    QButtonGroup dateRange_;
    QStandardItemModel *cboModel_;
    QCPTextElement *title_;
    QCPTextElement *info_;
    QCPFinancial *candlesticksChart_;
    QCPBars *volumeChart_;
    QCPAxisRect *volumeAxisRect_;
};

#endif // CHARTWIDGET_H
