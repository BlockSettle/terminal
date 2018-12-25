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


ChatWidget::~ChatWidget() = default;


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

   connect(client_.get(), &ChatClient::UserUpdate
           , usersViewModel_.get(), &ChatUsersViewModel::onUserUpdate);

   connect(ui_->send, &QPushButton::clicked, this, &ChatWidget::onSendButtonClicked);
   connect(ui_->text, &QLineEdit::returnPressed, this, &ChatWidget::onSendButtonClicked);
   connect(ui_->treeViewUsers, &QTreeView::clicked, this, &ChatWidget::onUserClicked);
   connect(client_.get(), &ChatClient::MessageUpdate, messagesViewModel_.get()
                        , &ChatMessagesViewModel::onMessage);
}


void ChatWidget::onUserClicked(const QModelIndex& index)
{
   QString userId = usersViewModel_->resolveUser(index);
   ui_->labelActiveChat->setText(tr("Block Settle Chat #") + userId);
   messagesViewModel_->onLeaveRoom();
   client_->onSetCurrentPrivateChat(userId);
}


void ChatWidget::onSendButtonClicked()
{
   QString messageText = ui_->text->text();

   if (!messageText.isEmpty())
   {
      client_->onSendMessage(messageText);
      ui_->text->clear();

      messagesViewModel_->onMessage(QDateTime::currentDateTime(), client_->prependMessage(messageText));
   }
}


std::string ChatWidget::login(const std::string& email, const std::string& jwt)
{
   try
   {
      logger_->debug("Set user name {}", email);
      usersViewModel_->onClear();
      std::string id = client_->loginToServer(email, jwt);
      ui_->stackedWidget->setCurrentIndex(1);
      return id;
   }
   catch (std::exception& e)
   {
      logger_->error("Caught an exception: {}" , e.what());
   }
   catch (...)
   {
      logger_->error("Unknown error ...");
   }
   return std::string();
}


void ChatWidget::logout()
{
   ui_->stackedWidget->setCurrentIndex(0);
   messagesViewModel_->onLeaveRoom();
   client_->logout();
}
