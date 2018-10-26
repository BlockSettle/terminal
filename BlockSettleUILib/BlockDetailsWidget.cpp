#include "BlockDetailsWidget.h"
#include "ui_BlockDetailsWidget.h"
#include <QIcon>


BlockDetailsWidget::BlockDetailsWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::BlockDetailsWidget)
{
    ui->setupUi(this);

    QIcon btcIcon(QLatin1String(":/ICON_BITCOIN_2X"));

    ui->icon->setPixmap(btcIcon.pixmap(32, 32));
}

BlockDetailsWidget::~BlockDetailsWidget()
{
    delete ui;
}
