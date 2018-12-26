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
   ui_->stackedWidget->setCurrentIndex(0);

   ui_->tableViewMessages->verticalHeader()->hide();
   ui_->tableViewMessages->horizontalHeader()->hide();
   ui_->tableViewMessages->setSelectionBehavior(QAbstractItemView::SelectRows);

   ui_->treeViewUsers->header()->hide();

   usersViewModel_.reset(new ChatUsersViewModel());
   ui_->treeViewUsers->setModel(usersViewModel_.get());

   messagesViewModel_.reset(new ChatMessagesViewModel());
   ui_->tableViewMessages->setModel(messagesViewModel_.get());
}


ChatWidget::~ChatWidget() = default;


void ChatWidget::init(const std::shared_ptr<ConnectionManager>& connectionManager
                 , const std::shared_ptr<ApplicationSettings> &appSettings
                 , const std::shared_ptr<spdlog::logger>& logger)
{
   logger_ = logger;
   client_ = std::make_shared<ChatClient>(connectionManager, appSettings, logger);

   connect(ui_->send, &QPushButton::clicked, this, &ChatWidget::onSendButtonClicked);
   connect(ui_->text, &QLineEdit::returnPressed, this, &ChatWidget::onSendButtonClicked);
   connect(ui_->treeViewUsers, &QTreeView::clicked, this, &ChatWidget::onUserClicked);

   connect(client_.get(), &ChatClient::UsersBeginUpdate
           , usersViewModel_.get(), &ChatUsersViewModel::onUsersBeginUpdate);
   connect(client_.get(), &ChatClient::UsersEndUpdate
           , usersViewModel_.get(), &ChatUsersViewModel::onUsersEndUpdate);
   connect(client_.get(), &ChatClient::UserUpdate
           , usersViewModel_.get(), &ChatUsersViewModel::onUserUpdate);

   connect(client_.get(), &ChatClient::MessagesBeginUpdate, messagesViewModel_.get()
                        , &ChatMessagesViewModel::onMessagesBeginUpdate);
   connect(client_.get(), &ChatClient::MessagesEndUpdate, messagesViewModel_.get()
                        , &ChatMessagesViewModel::onMessagesEndUpdate);
   connect(client_.get(), &ChatClient::MessageUpdate, messagesViewModel_.get()
                        , &ChatMessagesViewModel::onSequentialMessageUpdate);

   connect(messagesViewModel_.get(), &ChatMessagesViewModel::rowsInserted,
           this, &ChatWidget::onMessagesUpdated);
}


void ChatWidget::onUserClicked(const QModelIndex& index)
{
   if (!index.isValid())
       return;

   currentUserId_ = usersViewModel_->resolveUser(index);
   ui_->labelActiveChat->setText(tr("Block Settle Chat #") + currentUserId_);
   client_->onSetCurrentPrivateChat(currentUserId_);
   ui_->tableViewMessages->scrollToBottom();
}


void ChatWidget::onSendButtonClicked()
{
   QString messageText = ui_->text->text();

   if (!messageText.isEmpty())
   {
      client_->onSendMessage(messageText);
      ui_->text->clear();

      messagesViewModel_->onSingleMessageUpdate(QDateTime::currentDateTime(), client_->prependMessage(messageText));
      ui_->tableViewMessages->scrollToBottom();
   }
}


void ChatWidget::onMessagesUpdated(const QModelIndex& parent, int start, int end)
{
    auto selection = ui_->treeViewUsers->selectionModel();
    QModelIndex selectedUserIdx = usersViewModel_->resolveUser(currentUserId_);
    selection->select(selectedUserIdx, QItemSelectionModel::Select);
}


std::string ChatWidget::login(const std::string& email, const std::string& jwt)
{
   try
   {
      logger_->debug("Set user name {}", email);
      usersViewModel_->onClear();
      std::string userId = client_->loginToServer(email, jwt);
      currentUserId_ = QString::fromStdString(userId);
      ui_->stackedWidget->setCurrentIndex(1);

      ui_->labelActiveChat->setText(tr("Block Settle Chat #") + currentUserId_);
      client_->onSetCurrentPrivateChat(QString::fromStdString(userId));
      ui_->tableViewMessages->scrollToBottom();

      auto selection = ui_->treeViewUsers->selectionModel();
      QModelIndex selectedUserIdx = usersViewModel_->resolveUser(currentUserId_);
      selection->select(selectedUserIdx, QItemSelectionModel::Select);

      return userId;
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
   client_->logout();
   currentUserId_ = QString();
}
