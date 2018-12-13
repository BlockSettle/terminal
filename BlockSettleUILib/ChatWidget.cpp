#include "ChatWidget.h"
#include "ui_ChatWidget.h"


ChatWidget::ChatWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ChatWidget)
{
    ui->setupUi(this);
    //ui->messages->setModel(new QStringListModel(this));
}


ChatWidget::~ChatWidget()
{
}


void ChatWidget::init()
{
    // Put initialization here ...
}


void ChatWidget::setUserName(const QString& username)
{
    // Connect client ...
}


void ChatWidget::setUserId(const QString& userId)
{
    // Connect client ...
}


void ChatWidget::addLine(const QString &txt)
{
    int index = model->rowCount();
    model->insertRow(index);
    model->setData(model->index(index), txt);
}
