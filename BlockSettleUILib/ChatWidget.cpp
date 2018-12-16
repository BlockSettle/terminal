#include "ChatWidget.h"
#include "ui_ChatWidget.h"

#include "ChatClient.h"
#include "ChatServer.h"

#include <thread>
#include <spdlog/spdlog.h>

#include <QDebug>


ChatWidget::ChatWidget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ChatWidget)
{
    ui->setupUi(this);
}


ChatWidget::~ChatWidget()
{
}


void ChatWidget::init(const std::shared_ptr<ChatServer>& chatServer
                      , const std::shared_ptr<ConnectionManager>& connectionManager
                      , const std::shared_ptr<ApplicationSettings> &appSettings
                      , const std::shared_ptr<spdlog::logger>& logger)
{
    logger_ = logger;

    server_ = chatServer;

    serverPublicKey_ = server_->getPublicKey();

    client_ = std::make_shared<ChatClient>(connectionManager, appSettings, logger, serverPublicKey_);

    connect(ui->send, SIGNAL(clicked()), SLOT(onSendButtonClicked()));
    connect(ui->text, SIGNAL(returnPressed()), SLOT(onSendButtonClicked()));
}


void ChatWidget::populateUsers()
{

}


void ChatWidget::onSendButtonClicked()
{
    client_->sendMessage(ui->text->text());
    ui->text->clear();
}


void ChatWidget::setUserName(const QString& username)
{
    try
    {
        logger_->debug("Set user name {} - before", username.toStdString());
        client_->loginToServer("127.0.0.1", "20001", username.toStdString());
        logger_->debug("Set user name {} - after", username.toStdString());
        ui->stackedWidget->setCurrentIndex(1);
    }
    catch (std::exception& e)
    {
        logger_->error("Caught an exception: {}" , e.what());
    }
    catch (...)
    {
        logger_->error("Unknown error ...");
    }

}


void ChatWidget::logout()
{
    ui->stackedWidget->setCurrentIndex(0);
    client_->logout();
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
