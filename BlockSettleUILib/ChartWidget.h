#ifndef CHARTWIDGET_H
#define CHARTWIDGET_H

#include <QWidget>
#include <QButtonGroup>
#include "CommonTypes.h"
#include "qcustomplot.h"

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
	void rescalePlot();
	void OnMousePressed(QMouseEvent* event);
	void OnMouseReleased(QMouseEvent* event);

protected:
	void AddDataPoint(const qreal& open, const qreal& high, const qreal& low, const qreal& close, const qreal& timestamp, const qreal& volume) const;
	void UpdateChart(const int& interval) const;
	void InitializeCustomPlot();
	qreal IntervalWidth(int interval = -1, int count = 1) const;
	int FractionSizeForProduct(const QString &product) const;
	void ProcessProductsListResponse(const std::string& data);
	void ProcessOhlcHistoryResponse(const std::string& data);

	void setAutoScaleBtnColor() const;

	void AddNewCandle();
	void ModifyCandle();
	void UpdatePlot(const int& interval, const qint64& timestamp);

	void timerEvent(QTimerEvent* event);
	std::chrono::seconds getTimerInterval() const;

private:
	std::shared_ptr<ApplicationSettings>			appSettings_;
	std::shared_ptr<MarketDataProvider>				mdProvider_;
	std::shared_ptr<MdhsClient>						mdhsClient_;
	std::shared_ptr<spdlog::logger>					logger_;

	Ui::ChartWidget *ui_;
    QButtonGroup dateRange_;
    QStandardItemModel *cboModel_;
    QCPTextElement *title_;
    QCPTextElement *info_;
    QCPFinancial *candlesticksChart_;
    QCPBars *volumeChart_;
    QCPAxisRect *volumeAxisRect_;

	double lastHigh;
	double lastLow;
	double lastClose;
	double currentTimestamp;

	bool autoScaling{ true };

	qreal maxPrice;
	qreal minPrice;

	qreal newMaxPrice;
	qreal newMinPrice;

	qreal currentMinPrice{ 0 };
	qreal currentMaxPrice{ 0 };

	int timerId;
	int lastInterval;
	int dragY;

	bool isDraggingYAxis;
	bool isDraggingXAxis{ false };
	bool isDraggingMainPlot{ false };

	qreal lastDragCoordX{ 0.0 };
	qreal startDragCoordX{ 0.0 };
};

#endif // CHARTWIDGET_H
