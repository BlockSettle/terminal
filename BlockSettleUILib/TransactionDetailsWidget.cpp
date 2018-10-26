#include "TransactionDetailsWidget.h"
#include "ui_TransactionDetailsWidget.h"

TransactionDetailsWidget::TransactionDetailsWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::TransactionDetailsWidget)
{
    ui->setupUi(this);
}

TransactionDetailsWidget::~TransactionDetailsWidget()
{
    delete ui;
}
