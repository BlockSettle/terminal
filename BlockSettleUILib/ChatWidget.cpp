#include "ChatWidget.h"
#include "ui_ChatWidget.h"

#include "ChatClient.h"
#include "ChatServer.h"

#include <thread>


ChatWidget::ChatWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ChatWidget)
{
    ui->setupUi(this);
}


ChatWidget::~ChatWidget()
{
}


void ChatWidget::init(const std::shared_ptr<ConnectionManager>& connectionManager)
{
    server_ = std::make_shared<ChatServer>(connectionManager);

    std::thread([&]{
        server_->startServer("127.0.0.1", "20001");
    }).detach();

    client_ = std::make_shared<ChatClient>(connectionManager);
}


void ChatWidget::setUserName(const QString& username)
{
    client_->loginToServer("127.0.0.1", "20001", username.toStdString());
}


void ChatWidget::setUserId(const QString& userId)
{
    // Connect client ...
}


void ChatWidget::addMessage(const QString &txt)
{

}


void ChatWidget::addUser(const QString &txt)
{

}


void ChatWidget::addGroup(const QString &txt)
{

}
