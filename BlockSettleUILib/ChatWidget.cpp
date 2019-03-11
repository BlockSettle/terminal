#include "ChatWidget.h"
#include "ui_ChatWidget.h"

#include "ChatClient.h"
#include "ChatUsersViewModel.h"
#include "ChatUserListTreeWidget.h"
#include "ApplicationSettings.h"
#include "ChatSearchPopup.h"

#include <QScrollBar>
#include <QMouseEvent>
#include <QApplication>
#include <QObject>

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
   virtual void onUserClicked(const QString& userId) = 0;
   virtual void onMessagesUpdated() = 0;
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
   }

   std::string login(const std::string& email, const std::string& jwt) override {
      chat_->logger_->debug("Set user name {}", email);
      const auto userId = chat_->client_->loginToServer(email, jwt);
      chat_->ui_->textEditMessages->setOwnUserId(userId);
      chat_->ui_->labelUserName->setText(QString::fromStdString(userId));

      return userId;
   }

   void logout() override {
      chat_->logger_->info("Already logged out!");
   }

   void onSendButtonClicked()  override {
      qDebug("Send action when logged out");
   }

   void onUserClicked(const QString& /*userId*/)  override {}
   void onMessagesUpdated()  override {}
   void onLoginFailed()  override {
      chat_->changeState(ChatWidget::LoggedOut);
   }
   void onUsersDeleted(const std::vector<std::string> &) override {}
};

class ChatWidgetStateLoggedIn : public ChatWidgetState {
public:
   ChatWidgetStateLoggedIn(ChatWidget* parent) : ChatWidgetState(parent, ChatWidget::LoggedIn) {}

   void onStateEnter() override {}

   void onStateExit() override {
      chat_->onUserClicked({});
   }

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

         chat_->ui_->textEditMessages->onSingleMessageUpdate(msg);
      }
   }

   void onUserClicked(const QString& userId)  override {

      // save draft
      if (!chat_->currentChat_.isEmpty()) {
         QString messageText = chat_->ui_->input_textEdit->toPlainText();
         chat_->draftMessages_[chat_->currentChat_] = messageText;
      }

      chat_->currentChat_ = userId;
      chat_->ui_->input_textEdit->setEnabled(!chat_->currentChat_.isEmpty());
      chat_->ui_->labelActiveChat->setText(QObject::tr("CHAT #") + chat_->currentChat_);
      chat_->ui_->textEditMessages->onSwitchToChat(chat_->currentChat_);
      chat_->client_->retrieveUserMessages(chat_->currentChat_);

      // load draft
      if (chat_->draftMessages_.contains(userId)) {
         chat_->ui_->input_textEdit->setText(chat_->draftMessages_[userId]);
      } else {
         chat_->ui_->input_textEdit->setText(QLatin1Literal(""));
      }
   }

   void onMessagesUpdated()  override {
      QScrollBar *bar = chat_->ui_->textEditMessages->verticalScrollBar();
      bar->setValue(bar->maximum());
   }

   void onLoginFailed()  override {
      chat_->changeState(ChatWidget::LoggedOut);
   }

   void onUsersDeleted(const std::vector<std::string> &/*users*/)  override {}
};

ChatWidget::ChatWidget(QWidget *parent)
   : QWidget(parent)
   , ui_(new Ui::ChatWidget)
   , popup_(nullptr)
{
   ui_->setupUi(this);

   //Init UI and other stuff
   ui_->stackedWidget->setCurrentIndex(1); //Basically stackedWidget should be removed

   _chatUserListLogicPtr = std::make_shared<ChatUserListLogic>(this);

   qRegisterMetaType<std::vector<std::string>>();
}

ChatWidget::~ChatWidget() = default;

void ChatWidget::init(const std::shared_ptr<ConnectionManager>& connectionManager
                 , const std::shared_ptr<ApplicationSettings> &appSettings
                 , const std::shared_ptr<spdlog::logger>& logger)
{
   logger_ = logger;
   client_ = std::make_shared<ChatClient>(connectionManager, appSettings, logger);
   _chatUserListLogicPtr->init(client_, logger);

   connect(client_.get(), &ChatClient::LoginFailed, this, &ChatWidget::onLoginFailed);

   connect(ui_->send, &QPushButton::clicked, this, &ChatWidget::onSendButtonClicked);

   connect(ui_->treeWidgetUsers, &ChatUserListTreeWidget::userClicked, this, &ChatWidget::onUserClicked);

   connect(ui_->input_textEdit, &BSChatInput::sendMessage, this, &ChatWidget::onSendButtonClicked);

   connect(client_.get(), &ChatClient::UsersReplace,
           _chatUserListLogicPtr.get(), &ChatUserListLogic::onReplaceChatUsers);
   connect(client_.get(), &ChatClient::UsersAdd,
           _chatUserListLogicPtr.get(), &ChatUserListLogic::onAddChatUsers);
   connect(client_.get(), &ChatClient::UsersDel,
           _chatUserListLogicPtr.get(), &ChatUserListLogic::onRemoveChatUsers);
   connect(client_.get(), &ChatClient::IncomingFriendRequest,
           _chatUserListLogicPtr.get(), &ChatUserListLogic::onIcomingFriendRequest);
   connect(_chatUserListLogicPtr.get()->chatUserModelPtr().get(), &ChatUserModel::chatUserRemoved,
           this, &ChatWidget::onChatUserRemoved);

   connect(client_.get(), &ChatClient::MessagesUpdate, ui_->textEditMessages
                        , &ChatMessagesTextEdit::onMessagesUpdate);
   connect(ui_->textEditMessages, &ChatMessagesTextEdit::rowsInserted,
           this, &ChatWidget::onMessagesUpdated);
   
   connect(client_.get(), &ChatClient::MessageIdUpdated, ui_->textEditMessages
                        , &ChatMessagesTextEdit::onMessageIdUpdate);
   connect(client_.get(), &ChatClient::MessageStatusUpdated, ui_->textEditMessages
                        , &ChatMessagesTextEdit::onMessageStatusChanged);
   connect(ui_->textEditMessages, &ChatMessagesTextEdit::MessageRead,
           client_.get(), &ChatClient::onMessageRead);

   connect(ui_->chatSearchLineEdit, &ChatSearchLineEdit::returnPressed, this, &ChatWidget::onSearchUserReturnPressed);
   connect(_chatUserListLogicPtr->chatUserModelPtr().get(), &ChatUserModel::chatUserDataListChanged,
           ui_->treeWidgetUsers, &ChatUserListTreeWidget::onChatUserDataListChanged);

   changeState(State::LoggedOut); //Initial state is LoggedOut
}

void ChatWidget::onChatUserRemoved(const ChatUserDataPtr &chatUserDataPtr)
{
   if (currentChat_ == chatUserDataPtr->userId())
   {
      onUserClicked({});
   }
}

void ChatWidget::onUserClicked(const QString& userId)
{
   stateCurrent_->onUserClicked(userId);
}

void ChatWidget::onUsersDeleted(const std::vector<std::string> &users)
{
   stateCurrent_->onUsersDeleted(users);
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
         {
            stateCurrent_ = std::make_shared<ChatWidgetStateLoggedIn>(this);

            _chatUserListLogicPtr->readUsersFromDB();
         }
         break;
      case State::LoggedOut:
         {
            stateCurrent_ = std::make_shared<ChatWidgetStateLoggedOut>(this);
            _chatUserListLogicPtr->chatUserModelPtr()->resetModel();
            _chatUserListLogicPtr->readUsersFromDB();
         }
         break;
      }

      stateCurrent_->onStateEnter();
   }
}

void ChatWidget::onSendButtonClicked()
{
   return stateCurrent_->onSendButtonClicked();
}

void ChatWidget::onMessagesUpdated()
{
   return stateCurrent_->onMessagesUpdated();
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
              this, &ChatWidget::onAddUserToContacts);
      qApp->installEventFilter(this);
   }

   QString userToAdd = ui_->chatSearchLineEdit->text();
   if (!_chatUserListLogicPtr->chatUserModelPtr()->isChatUserExist(userToAdd))
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

void ChatWidget::onAddUserToContacts(const QString &userId)
{
   // check if user isn't already in contacts
   ChatUserModelPtr chatUserModelPtr = _chatUserListLogicPtr->chatUserModelPtr();

   if (chatUserModelPtr && !chatUserModelPtr->isChatUserInContacts(userId))
   {
      // add user to contacts as friend
      chatUserModelPtr->setUserState(userId, ChatUserData::State::Friend);
      ChatUserDataPtr chatUserDataPtr = chatUserModelPtr->getUserByUserId(userId);
      // save user in DB
      client_->addOrUpdateContact(chatUserDataPtr->userId(), chatUserDataPtr->userName());
      // and send friend request to ChatClient
      client_->sendFriendRequest(chatUserDataPtr->userId());
   }

   popup_->deleteLater();
   popup_ = nullptr;
   qApp->removeEventFilter(this);
}
