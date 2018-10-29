#ifndef __TRANSACTIONDETAILSWIDGET_H__
#define __TRANSACTIONDETAILSWIDGET_H__

#include <QWidget>
#include "BinaryData.h"

namespace Ui {
class TransactionDetailsWidget;
}
class QTreeWidget;
class QTreeWidgetItem;

class TransactionDetailsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TransactionDetailsWidget(QWidget *parent = nullptr);
    ~TransactionDetailsWidget();

    void setTxRefVal(const BinaryData& inTxRef);
    void loadInputs();

private:
    Ui::TransactionDetailsWidget *ui_;
    BinaryData txRefVal;
    QTreeWidgetItem * createItem(QTreeWidget *tree, QString type, QString address, QString amount, QString wallet);
    QTreeWidgetItem * createItem(QTreeWidgetItem *parentItem, QString type, QString address, QString amount, QString wallet);

};

#endif // TRANSACTIONDETAILSWIDGET_H
