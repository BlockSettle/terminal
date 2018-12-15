#define SPDLOG_DEBUG_ON

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


void ChatWidget::init(const std::shared_ptr<ConnectionManager>& connectionManager
                      , const std::shared_ptr<ApplicationSettings> &appSettings
                      , const std::shared_ptr<spdlog::logger>& logger)
{
    logger_ = logger;

    qDebug() << "ChatWidget::init!";
    server_ = std::make_shared<ChatServer>(connectionManager, appSettings, logger);

    serverPublicKey_ = server_->getPublicKey();

    server_->startServer("127.0.0.1", "20001");

    client_ = std::make_shared<ChatClient>(connectionManager, appSettings, logger, serverPublicKey_);
}


void ChatWidget::setUserName(const QString& username)
{
    qDebug() << "Try to login!";
    try
    {
        client_->loginToServer("127.0.0.1", "20001", username.toStdString());
    }
    catch (std::exception& e)
    {
        qDebug() << "Caught an exception: " << e.what();
    }
    catch (...)
    {
        qDebug() << "Unexpected error!";
    }

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
