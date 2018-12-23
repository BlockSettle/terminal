#include "ChatWidget.h"
#include "ui_ChatWidget.h"

#include "ChatClient.h"
#include "ChatUsersViewModel.h"
#include "ApplicationSettings.h"

#include <thread>
#include <spdlog/spdlog.h>

#include <QDebug>


ChatWidget::ChatWidget(QWidget *parent)
    : QWidget(parent)
    , ui_(new Ui::ChatWidget)
{
    ui_->setupUi(this);
}


ChatWidget::~ChatWidget()
{
}


void ChatWidget::init(const std::shared_ptr<ConnectionManager>& connectionManager
                      , const std::shared_ptr<ApplicationSettings> &appSettings
                      , const std::shared_ptr<spdlog::logger>& logger)
{
    logger_ = logger;

    client_ = std::make_shared<ChatClient>(connectionManager
                                           , appSettings
                                           , logger);

    usersViewModel_.reset(new ChatUsersViewModel());
    ui_->treeViewUsers->setModel(usersViewModel_.get());

    messagesViewModel_.reset(new ChatMessagesViewModel());
    ui_->tableViewMessages->setModel(messagesViewModel_.get());

    usersViewModel_->connect(client_.get(), SIGNAL(OnUserUpdate(const QString&))
                            , SLOT(onUserUpdate(const QString&)));

    connect(ui_->send, SIGNAL(clicked()), SLOT(onSendButtonClicked()));
    connect(ui_->text, SIGNAL(returnPressed()), SLOT(onSendButtonClicked()));
    connect(ui_->treeViewUsers, SIGNAL(doubleClicked(const QModelIndex&))
        , SLOT(onUserDoubleClicked(const QModelIndex&)));

    messagesViewModel_->connect(client_.get(), SIGNAL(OnMessageUpdate(const QDateTime&, const QString&))
                                , SLOT(onMessage(const QDateTime&, const QString&)));

}


void ChatWidget::onUserDoubleClicked(const QModelIndex& index)
{
    QString userId = usersViewModel_->resolveUser(index);
    ui_->labelActiveChat->setText(tr("Block Settle Chat #") + userId);
    messagesViewModel_->onLeaveRoom();
    client_->setCurrentPrivateChat(userId);
}


void ChatWidget::onSendButtonClicked()
{
    QString messageText = ui_->text->text();

    if (!messageText.isEmpty())
    {
        client_->sendMessage(messageText);
        ui_->text->clear();

        messagesViewModel_->onMessage(QDateTime::currentDateTime(), client_->prependMessage(messageText));
    }
}


void ChatWidget::login(const std::string& email, const std::string& jwt)
{
    try
    {
        logger_->debug("Set user name {}", email);
        usersViewModel_->clear();
        client_->loginToServer(email, jwt);
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


void ChatWidget::logout()
{
    ui_->stackedWidget->setCurrentIndex(0);
    messagesViewModel_->onLeaveRoom();
    client_->logout();
}
