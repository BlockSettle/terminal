#include "AddressDetailsWidget.h"
#include "ui_AddressDetailsWidget.h"

AddressDetailsWidget::AddressDetailsWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::AddressDetailsWidget)
{
    ui->setupUi(this);
}

AddressDetailsWidget::~AddressDetailsWidget()
{
    delete ui;
}
