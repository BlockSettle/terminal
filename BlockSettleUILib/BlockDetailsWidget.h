#ifndef __BLOCKDETAILSWIDGET_H__
#define __BLOCKDETAILSWIDGET_H__

#include <QWidget>

namespace Ui {
class BlockDetailsWidget;
}

class BlockDetailsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit BlockDetailsWidget(QWidget *parent = nullptr);
    ~BlockDetailsWidget();

private:
    Ui::BlockDetailsWidget *ui;
};

#endif // BLOCKDETAILSWIDGET_H
