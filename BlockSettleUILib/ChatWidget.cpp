#include "ChatWidget.h"
#include "ui_ChatWidget.h"


ChatWidget::ChatWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ChatWidget)
{
    ui->setupUi(this);
    ui->messages->setModel(new QStringListModel(this));
}


ChatWidget::~ChatWidget()
{
}


void ChatWidget::addLine(const QString &txt)
{
    int index = model->rowCount();
    model->insertRow(index);
    model->setData(model->index(index), txt);
}

