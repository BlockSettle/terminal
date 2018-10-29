#include "BlockDetailsWidget.h"
#include "ui_BlockDetailsWidget.h"
#include <QIcon>


BlockDetailsWidget::BlockDetailsWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::BlockDetailsWidget)
{
    ui->setupUi(this);

    QIcon btcIcon(QLatin1String(":/FULL_LOGO"));

    ui->icon->setPixmap(btcIcon.pixmap(80, 80));
}

BlockDetailsWidget::~BlockDetailsWidget()
{
    delete ui;
}
