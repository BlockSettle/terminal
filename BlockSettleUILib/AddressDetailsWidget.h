#ifndef __ADDRESSDETAILSWIDGET_H__
#define __ADDRESSDETAILSWIDGET_H__

#include <QWidget>
#include "Address.h"

namespace Ui {
class AddressDetailsWidget;
}

class AddressDetailsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AddressDetailsWidget(QWidget *parent = nullptr);
    ~AddressDetailsWidget();

    void setAddrVal(const bs::Address& inAddrVal);
    void loadTransactions();

private:
    Ui::AddressDetailsWidget *ui_;
    bs::Address addrVal;
};

#endif // ADDRESSDETAILSWIDGET_H
