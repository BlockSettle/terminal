#ifndef TRANSACTIONDETAILSWIDGET_H
#define TRANSACTIONDETAILSWIDGET_H

#include <QWidget>

namespace Ui {
class TransactionDetailsWidget;
}

class TransactionDetailsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TransactionDetailsWidget(QWidget *parent = nullptr);
    ~TransactionDetailsWidget();

private:
    Ui::TransactionDetailsWidget *ui;
};

#endif // TRANSACTIONDETAILSWIDGET_H
