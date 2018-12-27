#include "ChatWidget.h"
#include "ui_ChatWidget.h"

#include "ChatClient.h"
#include "ChatUsersViewModel.h"
#include "ApplicationSettings.h"

#include <thread>
#include <spdlog/spdlog.h>

#include <QDebug>


Q_DECLARE_METATYPE(std::vector<std::string>)


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

   qRegisterMetaType<std::vector<std::string>>();

}


ChatWidget::~ChatWidget() = default;


void ChatWidget::init(const std::shared_ptr<ConnectionManager>& connectionManager
                 , const std::shared_ptr<ApplicationSettings> &appSettings
                 , const std::shared_ptr<spdlog::logger>& logger)
{
   logger_ = logger;
   client_ = std::make_shared<ChatClient>(connectionManager, appSettings, logger);

   connect(client_.get(), &ChatClient::LoginFailed, this, &ChatWidget::onLoginFailed);

   connect(ui_->send, &QPushButton::clicked, this, &ChatWidget::onSendButtonClicked);
   connect(ui_->text, &QLineEdit::returnPressed, this, &ChatWidget::onSendButtonClicked);
   connect(ui_->treeViewUsers, &QTreeView::clicked, this, &ChatWidget::onUserClicked);

   connect(client_.get(), &ChatClient::UsersUpdate
           , usersViewModel_.get(), &ChatUsersViewModel::onUsersUpdate);
   connect(client_.get(), &ChatClient::MessagesUpdate, messagesViewModel_.get()
                        , &ChatMessagesViewModel::onMessagesUpdate);

   connect(messagesViewModel_.get(), &ChatMessagesViewModel::rowsInserted,
           this, &ChatWidget::onMessagesUpdated);
}


void ChatWidget::onUserClicked(const QModelIndex& index)
{
   if (!index.isValid())
       return;

   currentChatId_ = usersViewModel_->resolveUser(index);
   switchToChat(currentChatId_);
}


void ChatWidget::onSendButtonClicked()
{
   QString messageText = ui_->text->text();

   if (!messageText.isEmpty())
   {
      client_->onSendMessage(messageText);
      ui_->text->clear();

      if (currentUserId_ != currentChatId_)
      {
         messagesViewModel_->onSingleMessageUpdate(QDateTime::currentDateTime(), messageText);
      }
   }
}


void ChatWidget::onMessagesUpdated(const QModelIndex& parent, int start, int end)
{
   auto selection = ui_->treeViewUsers->selectionModel();
   if (selection)
   {
      QModelIndex selectedUserIdx = usersViewModel_->resolveUser(currentChatId_);
      selection->select(selectedUserIdx, QItemSelectionModel::Select);
      ui_->tableViewMessages->scrollToBottom();
   }
}


void ChatWidget::switchToChat(const QString& chatId)
{
   ui_->labelActiveChat->setText(tr("Chat #") + currentChatId_);
   client_->onSetCurrentPrivateChat(currentChatId_);
   messagesViewModel_->onSwitchToChat(currentChatId_);

   auto selection = ui_->treeViewUsers->selectionModel();
   if (selection)
   {
      QModelIndex selectedUserIdx = usersViewModel_->resolveUser(currentChatId_);
      selection->select(selectedUserIdx, QItemSelectionModel::Select);
   }
}


std::string ChatWidget::login(const std::string& email, const std::string& jwt)
{
   try
   {
      logger_->debug("Set user name {}", email);
      usersViewModel_->onClear();
      const std::string userId = client_->loginToServer(email, jwt);
      currentUserId_ = QString::fromStdString(userId);
      currentChatId_ = currentUserId_;
      ui_->stackedWidget->setCurrentIndex(1);

      switchToChat(currentChatId_);

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


void ChatWidget::onLoginFailed()
{
   ui_->stackedWidget->setCurrentIndex(0);
   currentChatId_ = QString();

   emit LoginFailed();
}


void ChatWidget::logout()
{
   ui_->stackedWidget->setCurrentIndex(0);
   client_->logout();
   currentChatId_ = QString();
}
