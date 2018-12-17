#include "ChatWidget.h"
#include "ui_ChatWidget.h"

#include "ChatClient.h"
#include "ChatServer.h"
#include "ChatUsersViewModel.h"

#include <thread>
#include <spdlog/spdlog.h>

#include <QDebug>


ChatWidget::ChatWidget(QWidget *parent) :
    QWidget(parent),
    ui_(new Ui::ChatWidget)
{
    ui_->setupUi(this);
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

    usersViewModel_.reset(new ChatUsersViewModel());
    ui_->treeViewUsers->setModel(usersViewModel_.get());

    usersViewModel_->connect(client_.get(), SIGNAL(OnUserUpdate(const QString&))
                            , SLOT(onUserUpdate(const QString&)));

    connect(ui_->send, SIGNAL(clicked()), SLOT(onSendButtonClicked()));
    connect(ui_->text, SIGNAL(returnPressed()), SLOT(onSendButtonClicked()));


    connect(ui_->treeViewUsers, SIGNAL(doubleClicked(const QModelIndex&))
        , SLOT(onUserDoubleClicked(const QModelIndex&)));
}


void ChatWidget::onUserDoubleClicked(const QModelIndex& index)
{
    ui_->labelActiveChat->setText(tr("Block Settle Chat #") + usersViewModel_->resolveUser(index));
}


void ChatWidget::onSendButtonClicked()
{
    client_->sendMessage(ui_->text->text());
    ui_->text->clear();
}


void ChatWidget::setUserName(const QString& username)
{
    try
    {
        logger_->debug("Set user name {} - before", username.toStdString());
        client_->loginToServer("127.0.0.1", "20001", username.toStdString());
        logger_->debug("Set user name {} - after", username.toStdString());
        ui_->stackedWidget->setCurrentIndex(1);
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


void ChatWidget::setUserId(const QString& userId)
{

}


void ChatWidget::logout()
{
    ui_->stackedWidget->setCurrentIndex(0);
    client_->logout();
}
