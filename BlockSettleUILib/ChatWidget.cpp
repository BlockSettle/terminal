#include "ChatWidget.h"
#include "ui_ChatWidget.h"

#include "ChatClient.h"
#include "ChatUsersViewModel.h"
#include "ApplicationSettings.h"

#include <thread>
#include <spdlog/spdlog.h>


Q_DECLARE_METATYPE(std::vector<std::string>)

ChatWidget::ChatWidget(QWidget *parent)
   : QWidget(parent)
   , ui_(new Ui::ChatWidget)
{
   ui_->setupUi(this);
   ui_->stackedWidget->setCurrentIndex(0);

   ui_->tableViewMessages->verticalHeader()->hide();
   ui_->tableViewMessages->verticalHeader()->setDefaultSectionSize(15);
   ui_->tableViewMessages->verticalHeader()->setSectionResizeMode(QHeaderView::Interactive);

   ui_->tableViewMessages->horizontalHeader()->hide();
   ui_->tableViewMessages->horizontalHeader()->setDefaultSectionSize(50);
   ui_->tableViewMessages->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
   ui_->tableViewMessages->setSelectionBehavior(QAbstractItemView::SelectRows);


   ui_->treeViewUsers->header()->hide();

   usersViewModel_.reset(new ChatUsersViewModel());
   ui_->treeViewUsers->setModel(usersViewModel_.get());

   messagesViewModel_.reset(new ChatMessagesViewModel());
   ui_->tableViewMessages->setModel(messagesViewModel_.get());

   qRegisterMetaType<std::vector<std::string>>();


   // 
   std::string label_color = "rgb(11, 22, 25)";
   std::string back_color = "rgb(28, 40, 53)";
   std::string send_button_color = "rgb(36, 125, 172)";
   ui_->tableViewMessages->setStyleSheet(QString::fromStdString("background-color:" + back_color + ";"));
   ui_->tableViewMessages->show();

   ui_->treeView_ChatRooms->setStyleSheet(QString::fromStdString("background-color:" + back_color + ";"));
   ui_->treeView_ChatRooms->show();

   ui_->treeViewUsers->setStyleSheet(QString::fromStdString("background-color:" + back_color + ";"));
   ui_->treeViewUsers->show();

   ui_->roomsFrame->setStyleSheet(QString::fromStdString("background-color:" + back_color + ";"));
   ui_->roomsFrame->show();

   ui_->chatFrame->setStyleSheet(QString::fromStdString("background-color:" + back_color + ";"));
   ui_->chatFrame->show();

   ui_->usersFrame->setStyleSheet(QString::fromStdString("background-color:" + back_color + ";"));
   ui_->usersFrame->show();

   //

   ui_->text->setStyleSheet(QString::fromStdString("background-color:" + back_color + ";"));
   ui_->text->show();

   ui_->send->setStyleSheet(QString::fromStdString("background-color:" + send_button_color + ";"));
   ui_->send->show();

   //

   ui_->label->setStyleSheet(QString::fromStdString("background-color:" + label_color + ";"));
   ui_->label->show();

   ui_->label_3->setStyleSheet(QString::fromStdString("background-color:" + label_color + ";"));
   ui_->label_3->show();

   ui_->labelActiveChat->setStyleSheet(QString::fromStdString("background-color:" + label_color + ";"));
   ui_->labelActiveChat->show();

   ui_->label_4->setStyleSheet(QString::fromStdString("background-color:" + back_color + ";"));
   ui_->label_4->show();

   ui_->label_5->setStyleSheet(QString::fromStdString("background-color:" + back_color + ";"));
   ui_->label_5->show();

   ui_->labelUserName->setStyleSheet(QString::fromStdString("background-color:" + back_color + ";"));
   ui_->labelUserName->show();

   //

   ui_->stackedWidget->setStyleSheet(QString::fromStdString("background-color:" + back_color + ";"));
   ui_->stackedWidget->show();



   ui_->page->setStyleSheet(QString::fromStdString("background-color:" + back_color + ";"));
   ui_->page->show();


   ui_->pageActive->setStyleSheet(QString::fromStdString("background-color:rgb(183, 187, 189);"));
   ui_->pageActive->show();

   ui_->tableViewMessages->setRowHeight(0, 10);
   ui_->tableViewMessages->setColumnWidth(1, 75);


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

   connect(client_.get(), &ChatClient::UsersReplace
           , usersViewModel_.get(), &ChatUsersViewModel::onUsersReplace);
   connect(client_.get(), &ChatClient::UsersAdd
      , usersViewModel_.get(), &ChatUsersViewModel::onUsersAdd);
   connect(client_.get(), &ChatClient::UsersDel
      , this, &ChatWidget::onUsersDeleted);

   connect(client_.get(), &ChatClient::MessagesUpdate, messagesViewModel_.get()
                        , &ChatMessagesViewModel::onMessagesUpdate);
   connect(messagesViewModel_.get(), &ChatMessagesViewModel::rowsInserted,
           this, &ChatWidget::onMessagesUpdated);
}

void ChatWidget::onUserClicked(const QModelIndex& index)
{
   currentChat_ = usersViewModel_->resolveUser(index);

   ui_->text->setEnabled(!currentChat_.isEmpty());
   ui_->labelActiveChat->setText(tr("CHAT #") + currentChat_);
   messagesViewModel_->onSwitchToChat(currentChat_);
}

void ChatWidget::onUsersDeleted(const std::vector<std::string> &users)
{
   usersViewModel_->onUsersDel(users);

   if (std::find(users.cbegin(), users.cend(), currentChat_.toStdString()) != users.cend()) {
      onUserClicked({});
   }
}

void ChatWidget::onSendButtonClicked()
{
   QString messageText = ui_->text->text();

   if (!messageText.isEmpty() && !currentChat_.isEmpty()) {
      client_->onSendMessage(messageText, currentChat_);
      ui_->text->clear();

      messagesViewModel_->onSingleMessageUpdate(QDateTime::currentDateTime(), messageText);
   }
}

void ChatWidget::onMessagesUpdated(const QModelIndex& parent, int start, int end)
{
   ui_->tableViewMessages->scrollToBottom();
}

std::string ChatWidget::login(const std::string& email, const std::string& jwt)
{
   try {
      logger_->debug("Set user name {}", email);
      usersViewModel_->onUsersReplace({});
      const auto userId = client_->loginToServer(email, jwt);
      ui_->stackedWidget->setCurrentIndex(1);
      ui_->labelUserName->setText(QString::fromStdString(userId));
      messagesViewModel_->setOwnUserId(userId);

      return userId;
   }
   catch (std::exception& e) {
      logger_->error("Caught an exception: {}" , e.what());
   }
   catch (...) {
      logger_->error("Unknown error ...");
   }
   return std::string();
}

void ChatWidget::onLoginFailed()
{
   ui_->stackedWidget->setCurrentIndex(0);

   emit LoginFailed();
}

void ChatWidget::logout()
{
   ui_->stackedWidget->setCurrentIndex(0);
   client_->logout();
}
