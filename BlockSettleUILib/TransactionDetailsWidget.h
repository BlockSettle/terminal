#ifndef __TRANSACTIONDETAILSWIDGET_H__
#define __TRANSACTIONDETAILSWIDGET_H__

#include <QWidget>
#include "BinaryData.h"

namespace Ui {
class TransactionDetailsWidget;
}
class CustomTreeWidget;
class QTreeWidgetItem;
class QTreeWidget;

class TransactionDetailsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TransactionDetailsWidget(QWidget *parent = nullptr);
    ~TransactionDetailsWidget();

    void setTxRefVal(const BinaryData& inTxRef);
    void setTxVal(const QString inTx); // possibly a temporary function to show workflow
    void loadInputs();

    enum TxTreeColumns {
       colType = 0,
       colAddressId = 1,
       colAmount = 2,
       colWallet
    };

signals:
   void addressClicked(QString addressId);

protected slots:
   void onAddressClicked(QTreeWidgetItem *item, int column);

protected:
   void loadTree(CustomTreeWidget *tree);

private:
    Ui::TransactionDetailsWidget *ui_;
    BinaryData txRefVal;
    QTreeWidgetItem * createItem(QTreeWidget *tree, QString type, QString address, QString amount, QString wallet);
    QTreeWidgetItem * createItem(QTreeWidgetItem *parentItem, QString type, QString address, QString amount, QString wallet);

};

#endif // TRANSACTIONDETAILSWIDGET_H
