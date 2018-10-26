#include "BlockDetailsWidget.h"
#include "ui_BlockDetailsWidget.h"

BlockDetailsWidget::BlockDetailsWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::BlockDetailsWidget)
{
    ui->setupUi(this);
}

BlockDetailsWidget::~BlockDetailsWidget()
{
    delete ui;
}
