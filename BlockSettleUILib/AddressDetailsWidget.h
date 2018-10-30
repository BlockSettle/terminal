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
    void loadTransactions();

 signals:
    void transactionClicked(QString txId);

protected slots:
   void onSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected);
   void onItemClicked(QTreeWidgetItem *item, int column);


private:
    Ui::AddressDetailsWidget *ui_;
    bs::Address addrVal;
    void setConfirmationColor(QTreeWidgetItem *item);

};

#endif // ADDRESSDETAILSWIDGET_H
