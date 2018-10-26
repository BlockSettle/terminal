#ifndef ADDRESSDETAILSWIDGET_H
#define ADDRESSDETAILSWIDGET_H

#include <QWidget>

namespace Ui {
class AddressDetailsWidget;
}

class AddressDetailsWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AddressDetailsWidget(QWidget *parent = nullptr);
    ~AddressDetailsWidget();

private:
    Ui::AddressDetailsWidget *ui;
};

#endif // ADDRESSDETAILSWIDGET_H
