#include "ChartWidget.h"
#include "ui_ChartWidget.h"

ChartWidget::ChartWidget(QWidget *parent)
   : QWidget(parent)
   , ui_(new Ui::ChartWidget) {
   ui_->setupUi(this);

   setStyling();
   QCandlestickSeries *series = new QCandlestickSeries();
   series->setIncreasingColor(QColor(Qt::darkGreen));
   series->setDecreasingColor(QColor(Qt::red));
   QDateTime dt;
   dt.setDate(QDate(2018, 10, 1));
   series->append(new QCandlestickSet(10.0, 12.0, 9.0, 11.0, dt.toTime_t(), series));
   dt.setDate(QDate(2018, 10, 2));
   series->append(new QCandlestickSet(12.0, 15.0, 11.0, 12.0, dt.toTime_t(), series));
   dt.setDate(QDate(2018, 10, 3));
   series->append(new QCandlestickSet(10.0, 12.0, 9.0, 13.0, dt.toTime_t(), series));
   dt.setDate(QDate(2018, 10, 4));
   series->append(new QCandlestickSet(12.5, 12.2, 9.9, 10.0, dt.toTime_t(), series));

   ui_->viewMain->chart()->addSeries(series);
   ui_->viewMain->chart()->createDefaultAxes();
}

ChartWidget::~ChartWidget() {
    delete ui_;
}

void ChartWidget::setStyling() {
   QBrush bgBrush(QColor(24, 32, 43));
   ui_->viewMain->chart()->setBackgroundBrush(bgBrush);
   ui_->viewVolume->chart()->setBackgroundBrush(bgBrush);
}
