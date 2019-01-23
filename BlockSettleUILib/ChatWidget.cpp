#include "ChatWidget.h"
#include "ui_ChatWidget.h"

#include "ChatClient.h"
#include "ChatUsersViewModel.h"
#include "ApplicationSettings.h"

#include <thread>
#include <spdlog/spdlog.h>


Q_DECLARE_METATYPE(std::vector<std::string>)

class ChatWidgetState {
public:
	virtual void onStateEnter() {}; //Do something special on state appears, by default nothing
	virtual void onStateExit() {}; //Do something special on state about to gone, by default nothing

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

	ChatWidget::State type() { return type_; };

protected:
	ChatWidget * chat_;
private:
	ChatWidget::State type_;
};

class ChatWidgetStateLoggedOut : public ChatWidgetState {
public:
	ChatWidgetStateLoggedOut(ChatWidget* parent) : ChatWidgetState(parent, ChatWidget::LoggedOut) {};

	virtual void onStateEnter() override {
		chat_->logger_->debug("Set user name {}", "<empty>");
		chat_->usersViewModel_->onUsersReplace({});
		chat_->ui_->labelUserName->setText(QString::fromStdString(""));
	}

	virtual std::string login(const std::string& email, const std::string& jwt) override {
		chat_->logger_->debug("Set user name {}", email);
		chat_->usersViewModel_->onUsersReplace({});
		const auto userId = chat_->client_->loginToServer(email, jwt);
		chat_->ui_->labelUserName->setText(QString::fromStdString(userId));
		chat_->messagesViewModel_->setOwnUserId(userId);

		return userId;
	};
	virtual void logout() override {
		chat_->logger_->info("Already logged out!");
	};
	virtual void onSendButtonClicked()  override {
		
	};
	virtual void onUserClicked(const QModelIndex& index)  override {};
	virtual void onMessagesUpdated(const QModelIndex& parent, int start, int end)  override {};
	virtual void onLoginFailed()  override {
		chat_->changeState(ChatWidget::LoggedOut);
	};
	virtual void onUsersDeleted(const std::vector<std::string> &) override {};
};

class ChatWidgetStateLoggedIn : public ChatWidgetState {
public:
	ChatWidgetStateLoggedIn(ChatWidget* parent) : ChatWidgetState(parent, ChatWidget::LoggedIn) {};

	virtual void onStateEnter() override {};

	virtual std::string login(const std::string& email, const std::string& jwt) override {
		chat_->logger_->info("Already logged in! You should first logout!");
		return std::string();
	};
	virtual void logout() override {	
		chat_->client_->logout();
		chat_->changeState(ChatWidget::LoggedOut);
	};
	virtual void onSendButtonClicked()  override {
		//QString messageText = chat_->ui_->text->text();
		QString messageText = chat_->ui_->input_textEdit->toPlainText();

		if (!messageText.isEmpty() && !chat_->currentChat_.isEmpty()) {
			auto msg = chat_->client_->SendOwnMessage(messageText, chat_->currentChat_);
			//chat_->ui_->text->clear();
			chat_->ui_->input_textEdit->clear();

			chat_->messagesViewModel_->onSingleMessageUpdate(msg);
		}
	};
	virtual void onUserClicked(const QModelIndex& index)  override {
		chat_->currentChat_ = chat_->usersViewModel_->resolveUser(index);

		//chat_->ui_->text->setEnabled(!chat_->currentChat_.isEmpty());
		chat_->ui_->input_textEdit->setEnabled(!chat_->currentChat_.isEmpty());
		chat_->ui_->labelActiveChat->setText(QObject::tr("CHAT #") + chat_->currentChat_);
		chat_->messagesViewModel_->onSwitchToChat(chat_->currentChat_);
		chat_->client_->retrieveUserMessages(chat_->currentChat_);
	};
	virtual void onMessagesUpdated(const QModelIndex& parent, int start, int end)  override {
		chat_->ui_->tableViewMessages->scrollToBottom();
	};
	virtual void onLoginFailed()  override {
		chat_->changeState(ChatWidget::LoggedOut);
	};
	virtual void onUsersDeleted(const std::vector<std::string> &users)  override {
		chat_->usersViewModel_->onUsersDel(users);

		if (std::find(users.cbegin(), users.cend(), chat_->currentChat_.toStdString()) != users.cend()) {
			chat_->onUserClicked({});
		}
	};
};

ChatWidget::ChatWidget(QWidget *parent)
   : QWidget(parent)
   , ui_(new Ui::ChatWidget)
{
   ui_->setupUi(this);

   //Init UI and other stuff
   ui_->stackedWidget->setCurrentIndex(1); //Basically stackedWidget should be removed

   ui_->tableViewMessages->verticalHeader()->hide();
   ui_->tableViewMessages->verticalHeader()->setDefaultSectionSize(15);
   ui_->tableViewMessages->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

   ui_->tableViewMessages->horizontalHeader()->hide();
   ui_->tableViewMessages->horizontalHeader()->setDefaultSectionSize(50);
   ui_->tableViewMessages->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
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
   //connect(ui_->text, &QLineEdit::returnPressed, this, &ChatWidget::onSendButtonClicked);
   connect(ui_->treeViewUsers, &QTreeView::clicked, this, &ChatWidget::onUserClicked);
   ui_->input_textEdit->installEventFilter(this);
   //connect(ui_->input_textEdit, &QTextEdit::returnPressed, this, &ChatWidget::onSendButtonClicked);

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

   changeState(State::LoggedOut); //Initial state is LoggedOut

}

void ChatWidget::onUserClicked(const QModelIndex& index)
{
   return stateCurrent_->onUserClicked(index); //test

   currentChat_ = usersViewModel_->resolveUser(index);

   //ui_->text->setEnabled(!currentChat_.isEmpty());
   ui_->input_textEdit->setEnabled(!currentChat_.isEmpty());
   ui_->labelActiveChat->setText(tr("CHAT #") + currentChat_);
   messagesViewModel_->onSwitchToChat(currentChat_);
   client_->retrieveUserMessages(currentChat_);
}

void ChatWidget::onUsersDeleted(const std::vector<std::string> &users)
{
   return stateCurrent_->onUsersDeleted(users); //test

   usersViewModel_->onUsersDel(users);

   if (std::find(users.cbegin(), users.cend(), currentChat_.toStdString()) != users.cend()) {
      onUserClicked({});
   }
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

#include <QKeyEvent>

bool ChatWidget::eventFilter(QObject * obj, QEvent * event)
{
	qDebug("Event %d", event->type());
	if (event->type() == QEvent::KeyPress) {
		QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event); 
		//qDebug("Ate key press %d", keyEvent->key());
		switch (keyEvent->key()) {
		case Qt::Key_Enter:
			return true;
		default:
			return QObject::eventFilter(obj, event);
		}
		return QObject::eventFilter(obj, event);
	}
	else {
		// standard event processing
		return QObject::eventFilter(obj, event);
	}
	return false;
}

void ChatWidget::onSendButtonClicked()
{
   return stateCurrent_->onSendButtonClicked(); //test
   /*
   QString messageText = ui_->text->text();

   if (!messageText.isEmpty() && !currentChat_.isEmpty()) {
      auto msg = client_->SendOwnMessage(messageText, currentChat_);
      ui_->text->clear();

      messagesViewModel_->onSingleMessageUpdate(msg);
   }
   */
}

void ChatWidget::onMessagesUpdated(const QModelIndex& parent, int start, int end)
{
   return stateCurrent_->onMessagesUpdated(parent, start, end); //test
   ui_->tableViewMessages->scrollToBottom();
}

std::string ChatWidget::login(const std::string& email, const std::string& jwt)
{
   try {
	  const auto userId1 = stateCurrent_->login(email, jwt); //test
	  changeState(State::LoggedIn);
	  return userId1;

	  
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
   stateCurrent_->onLoginFailed(); //test

   //ui_->stackedWidget->setCurrentIndex(0);

   emit LoginFailed();
}

void ChatWidget::logout()
{
   return stateCurrent_->logout(); //test
   ui_->stackedWidget->setCurrentIndex(0);
   client_->logout();
}
