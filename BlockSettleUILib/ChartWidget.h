#ifndef CHARTWIDGET_H
#define CHARTWIDGET_H

#include <QWidget>
#include <QtCharts>

namespace Ui {
class ChartWidget;
}

using namespace QtCharts;

class ChartWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ChartWidget(QWidget *parent = nullptr);
    ~ChartWidget();

protected:
   void setStyling();

private:
    Ui::ChartWidget *ui_;
};

#endif // CHARTWIDGET_H
