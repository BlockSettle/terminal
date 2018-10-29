#ifndef __TRANSACTIONDETAILSWIDGET_H__
#define __TRANSACTIONDETAILSWIDGET_H__

#include <QWidget>
#include "BinaryData.h"

namespace Ui {
class TransactionDetailsWidget;
}

class TransactionDetailsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TransactionDetailsWidget(QWidget *parent = nullptr);
    ~TransactionDetailsWidget();

    void setTxRefVal(const BinaryData& inTxRef) { txRefVal = inTxRef; }

private:
    Ui::TransactionDetailsWidget *ui;
    BinaryData txRefVal;
};

#endif // TRANSACTIONDETAILSWIDGET_H
