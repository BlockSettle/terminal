#include "ChatWidget.h"
#include "ui_ChatWidget.h"

#include "ChatClient.h"
#include "ChatUsersViewModel.h"
#include "ApplicationSettings.h"
#include "ChatSearchPopup.h"

#include <QMouseEvent>
#include <QApplication>
#include <QObject>
#include <QtDebug>

#include <thread>
#include <spdlog/spdlog.h>


Q_DECLARE_METATYPE(std::vector<std::string>)

class ChatWidgetState {
public:
    virtual void onStateEnter() {} //Do something special on state appears, by default nothing
    virtual void onStateExit() {} //Do something special on state about to gone, by default nothing

public:

   explicit ChatWidgetState(ChatWidget* chat, ChatWidget::State type) : chat_(chat), type_(type) {}
   virtual ~ChatWidgetState() = default;

   virtual std::string login(const std::string& email, const std::string& jwt) = 0;
   virtual void logout() = 0;
   virtual void onSendButtonClicked() = 0;
   virtual void onUserClicked(const QModelIndex& index) = 0;
   virtual void onMessagesUpdated(const QModelIndex& parent, int start, int end) = 0;
   virtual void onLoginFailed() = 0;
   virtual void onUsersDeleted(const std::vector<std::string> &) = 0;

    ChatWidget::State type() { return type_; }

protected:
   ChatWidget * chat_;
private:
   ChatWidget::State type_;
};

class ChatWidgetStateLoggedOut : public ChatWidgetState {
public:
    ChatWidgetStateLoggedOut(ChatWidget* parent) : ChatWidgetState(parent, ChatWidget::LoggedOut) {}

   virtual void onStateEnter() override {
      chat_->logger_->debug("Set user name {}", "<empty>");
      chat_->usersViewModel_->onUsersReplace({});
   }

   std::string login(const std::string& email, const std::string& jwt) override {
      chat_->logger_->debug("Set user name {}", email);
      chat_->usersViewModel_->onUsersReplace({});
      const auto userId = chat_->client_->loginToServer(email, jwt);
      chat_->messagesViewModel_->setOwnUserId(userId);

      return userId;
    }
   void logout() override {
      chat_->logger_->info("Already logged out!");
    }
   void onSendButtonClicked()  override {
      qDebug("Send action when logged out");
    }
    void onUserClicked(const QModelIndex& /*index*/)  override {}
    void onMessagesUpdated(const QModelIndex& /*parent*/, int /*start*/, int /*end*/)  override {}
   void onLoginFailed()  override {
      chat_->changeState(ChatWidget::LoggedOut);
    }
    void onUsersDeleted(const std::vector<std::string> &) override {}
};

class ChatWidgetStateLoggedIn : public ChatWidgetState {
public:
    ChatWidgetStateLoggedIn(ChatWidget* parent) : ChatWidgetState(parent, ChatWidget::LoggedIn) {}

    void onStateEnter() override {}

    std::string login(const std::string& /*email*/, const std::string& /*jwt*/) override {
      chat_->logger_->info("Already logged in! You should first logout!");
      return std::string();
    }
   void logout() override {
      chat_->client_->logout();
      chat_->changeState(ChatWidget::LoggedOut);
    }
   void onSendButtonClicked()  override {
      QString messageText = chat_->ui_->input_textEdit->toPlainText();

      if (!messageText.isEmpty() && !chat_->currentChat_.isEmpty()) {
         auto msg = chat_->client_->sendOwnMessage(messageText, chat_->currentChat_);
         chat_->ui_->input_textEdit->clear();

         chat_->messagesViewModel_->onSingleMessageUpdate(msg);
      }
    }
   void onUserClicked(const QModelIndex& index)  override {
      chat_->currentChat_ = chat_->usersViewModel_->resolveUser(index);

      chat_->ui_->input_textEdit->setEnabled(!chat_->currentChat_.isEmpty());
      chat_->ui_->labelActiveChat->setText(QObject::tr("CHAT #") + chat_->currentChat_);
      chat_->messagesViewModel_->onSwitchToChat(chat_->currentChat_);
      chat_->client_->retrieveUserMessages(chat_->currentChat_);
    }
    void onMessagesUpdated(const QModelIndex& /*parent*/, int /*start*/, int /*end*/)  override {
      chat_->ui_->tableViewMessages->scrollToBottom();
    }
   void onLoginFailed()  override {
      chat_->changeState(ChatWidget::LoggedOut);
    }
   void onUsersDeleted(const std::vector<std::string> &users)  override {
      chat_->usersViewModel_->onUsersDel(users);

      if (std::find(users.cbegin(), users.cend(), chat_->currentChat_.toStdString()) != users.cend()) {
         chat_->onUserClicked({});
      }
    }
};

ChatWidget::ChatWidget(QWidget *parent)
   : QWidget(parent)
   , ui_(new Ui::ChatWidget)
   , popup_(nullptr)
{
   ui_->setupUi(this);

   //Init UI and other stuff
   ui_->stackedWidget->setCurrentIndex(1); //Basically stackedWidget should be removed

   ui_->tableViewMessages->verticalHeader()->setDefaultSectionSize(15);
   ui_->tableViewMessages->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

   ui_->tableViewMessages->horizontalHeader()->setDefaultSectionSize(50);
   ui_->tableViewMessages->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
   ui_->tableViewMessages->setSelectionBehavior(QAbstractItemView::SelectRows);
   ui_->tableViewMessages->horizontalHeader()->setDefaultAlignment(Qt::AlignLeading | Qt::AlignVCenter);

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
   connect(ui_->treeViewUsers, &QTreeView::clicked, this, &ChatWidget::onUserClicked);

   connect(ui_->input_textEdit, &BSChatInput::sendMessage, this, &ChatWidget::onSendButtonClicked);

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

   connect(ui_->chatSearchLineEdit, &ChatSearchLineEdit::returnPressed, this, &ChatWidget::onSearchUserReturnPressed);

   changeState(State::LoggedOut); //Initial state is LoggedOut

}

void ChatWidget::onUserClicked(const QModelIndex& index)
{
   return stateCurrent_->onUserClicked(index);
}

void ChatWidget::onUsersDeleted(const std::vector<std::string> &users)
{
   return stateCurrent_->onUsersDeleted(users);
}

void ChatWidget::changeState(ChatWidget::State state)
{
   if (!stateCurrent_) { //In case if we use change state in first time
      stateCurrent_ = std::make_shared<ChatWidgetStateLoggedOut>(this);
      stateCurrent_->onStateEnter();
   } else if (stateCurrent_->type() != state) {
      stateCurrent_->onStateExit();

      switch (state) {
      case State::LoggedIn:
         stateCurrent_ = std::make_shared<ChatWidgetStateLoggedIn>(this);
         break;
      case State::LoggedOut:
         stateCurrent_ = std::make_shared<ChatWidgetStateLoggedOut>(this);
         break;
      }

      stateCurrent_->onStateEnter();
   }
}

void ChatWidget::onSendButtonClicked()
{
   return stateCurrent_->onSendButtonClicked();
}

void ChatWidget::onMessagesUpdated(const QModelIndex& parent, int start, int end)
{
   return stateCurrent_->onMessagesUpdated(parent, start, end);
}

std::string ChatWidget::login(const std::string& email, const std::string& jwt)
{
   try {
     const auto userId = stateCurrent_->login(email, jwt);
     changeState(State::LoggedIn);
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
   stateCurrent_->onLoginFailed();
   emit LoginFailed();
}

void ChatWidget::logout()
{
   return stateCurrent_->logout(); //test
}

void ChatWidget::onSearchUserReturnPressed()
{
   if (!popup_)
   {
      popup_ = new ChatSearchPopup(this);
      connect(popup_, &ChatSearchPopup::addUserToContacts,
         [this](const QString &)
         {
            // TODO: find user and add to contact list
            popup_->deleteLater();
            popup_ = nullptr;
         }
      );
      qApp->installEventFilter(this);
   }

   QString userToAdd = ui_->chatSearchLineEdit->text();
   if (!usersViewModel_.get()->isUserInModel(userToAdd.toStdString()))
       return;

   popup_->setText(userToAdd);
   popup_->setGeometry(0, 0, ui_->chatSearchLineEdit->width(), static_cast<int>(ui_->chatSearchLineEdit->height() * 1.2));
   popup_->setCustomPosition(ui_->chatSearchLineEdit, 0, 5);
   popup_->show();
}

bool ChatWidget::eventFilter(QObject *obj, QEvent *event)
{
   if (!popup_)
      return QWidget::eventFilter(obj, event);

   if (obj == this->window()
       && event->type() == QEvent::Move)
   {
      popup_->move(ui_->chatSearchLineEdit->mapToGlobal(ui_->chatSearchLineEdit->rect().bottomLeft()));
   }

   if (event->type() == QEvent::MouseButtonRelease)
   {
      QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
      QPoint pos = mouseEvent->pos();

      if (!popup_->rect().contains(pos))
      {
         qApp->removeEventFilter(this);
         popup_->deleteLater();
         popup_ = nullptr;
      }
   }

   return QWidget::eventFilter(obj, event);
}

