#ifndef __ADDRESSDETAILSWIDGET_H__
#define __ADDRESSDETAILSWIDGET_H__

#include <QWidget>
#include "Address.h"
#include <QItemSelection>

namespace Ui {
class AddressDetailsWidget;
}
class QTreeWidgetItem;

class AddressDetailsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AddressDetailsWidget(QWidget *parent = nullptr);
    ~AddressDetailsWidget();

    void setAddrVal(const bs::Address& inAddrVal);
    void setAddrVal(const QString inAddrVal);
    void loadTransactions();

    enum AddressTreeColumns {
       colDate = 0,
       colTxId = 1,
       colConfs = 2,
       colInputsNum,
       colOutputsNum,
       colOutput,
       colFees,
       colFeePerByte,
       colSize
    };

 signals:
    void transactionClicked(QString txId);

protected slots:
   void onTxClicked(QTreeWidgetItem *item, int column);

private:
    Ui::AddressDetailsWidget *ui_;
    bs::Address addrVal;
    void setConfirmationColor(QTreeWidgetItem *item);
    void setOutputColor(QTreeWidgetItem *item);
};

#endif // ADDRESSDETAILSWIDGET_H
