#include "ChatWidget.h"
#include "ui_ChatWidget.h"

#include "ChatClient.h"
#include "ApplicationSettings.h"
#include "ChatSearchPopup.h"

#include "OTCRequestViewModel.h"

#include <QScrollBar>
#include <QMouseEvent>
#include <QApplication>
#include <QObject>
#include <QDebug>
#include "UserHasher.h"

#include <thread>
#include <spdlog/spdlog.h>
#include "ChatClientDataModel.h"


Q_DECLARE_METATYPE(std::vector<std::string>)


enum class OTCPages : int
{
   OTCCreateRequest = 0,
   OTCCreateResponse,
   OTCNegotiateRequest,
   OTCNegotiateResponse
};

constexpr int kShowEmptyFoundUserListTimeoutMs = 3000;

class ChatWidgetState {
public:
    virtual void onStateEnter() {} //Do something special on state appears, by default nothing
    virtual void onStateExit() {} //Do something special on state about to gone, by default nothing

public:

   explicit ChatWidgetState(ChatWidget* chat, ChatWidget::State type) : chat_(chat), type_(type) {}
   virtual ~ChatWidgetState() = default;

   virtual std::string login(const std::string& email, const std::string& jwt) = 0;
   virtual void logout() = 0;
   virtual void onLoggedOut() { }
   virtual void onSendButtonClicked() = 0;
   virtual void onUserClicked(const QString& userId) = 0;
   virtual void onMessagesUpdated() = 0;
   virtual void onLoginFailed() = 0;
   virtual void onUsersDeleted(const std::vector<std::string> &) = 0;
   virtual void onRoomClicked(const QString& userId) = 0;

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
      chat_->ui_->input_textEdit->setText(QLatin1Literal(""));
      chat_->ui_->input_textEdit->setVisible(false);
      chat_->ui_->input_textEdit->setEnabled(false);
      chat_->ui_->chatSearchLineEdit->clear();
      chat_->ui_->chatSearchLineEdit->setEnabled(false);
      chat_->ui_->labelUserName->setText(QLatin1String("offline"));
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
   void onRoomClicked(const QString& /*roomId*/)  override {}
   void onMessagesUpdated()  override {}
   void onLoginFailed()  override {
      chat_->changeState(ChatWidget::LoggedOut);
   }
   void onUsersDeleted(const std::vector<std::string> &) override {}
};

class ChatWidgetStateLoggedIn : public ChatWidgetState {
public:
   ChatWidgetStateLoggedIn(ChatWidget* parent) : ChatWidgetState(parent, ChatWidget::LoggedIn) {}

   void onStateEnter() override {
      chat_->ui_->input_textEdit->setText(QLatin1Literal(""));
      chat_->ui_->input_textEdit->setVisible(true);
      chat_->ui_->input_textEdit->setEnabled(true);
      chat_->ui_->chatSearchLineEdit->setEnabled(true);
   }

   void onStateExit() override {
      chat_->onUserClicked({});
   }

   std::string login(const std::string& /*email*/, const std::string& /*jwt*/) override {
      chat_->logger_->info("Already logged in! You should first logout!");
      return std::string();
   }

   void logout() override {
      chat_->client_->logout();
   }

   void onLoggedOut() override {
      chat_->changeState(ChatWidget::LoggedOut);
   }

   void onSendButtonClicked()  override {
      QString messageText = chat_->ui_->input_textEdit->toPlainText();

      if (!messageText.isEmpty() && !chat_->currentChat_.isEmpty()) {
         if (!chat_->isRoom()){
            auto msg = chat_->client_->sendOwnMessage(messageText, chat_->currentChat_);
            chat_->ui_->input_textEdit->clear();
         } else {
            auto msg = chat_->client_->sendRoomOwnMessage(messageText, chat_->currentChat_);
            chat_->ui_->input_textEdit->clear();
         }
      }
   }

   void onUserClicked(const QString& userId)  override {

      chat_->ui_->stackedWidgetMessages->setCurrentIndex(0);

      // save draft
      if (!chat_->currentChat_.isEmpty()) {
         QString messageText = chat_->ui_->input_textEdit->toPlainText();
         chat_->draftMessages_[chat_->currentChat_] = messageText;
      }

      chat_->currentChat_ = userId;
      chat_->setIsRoom(false);
      chat_->ui_->input_textEdit->setEnabled(!chat_->currentChat_.isEmpty());
      chat_->ui_->labelActiveChat->setText(QObject::tr("CHAT #") + chat_->currentChat_);
      chat_->ui_->textEditMessages->switchToChat(chat_->currentChat_);
      chat_->client_->retrieveUserMessages(chat_->currentChat_);

      // load draft
      if (chat_->draftMessages_.contains(userId)) {
         chat_->ui_->input_textEdit->setText(chat_->draftMessages_[userId]);
      } else {
         chat_->ui_->input_textEdit->setText(QLatin1Literal(""));
      }
      chat_->ui_->input_textEdit->setFocus();
   }

   void onRoomClicked(const QString& roomId) override {
      if (roomId == QLatin1Literal("otc_chat")) {
         chat_->ui_->stackedWidgetMessages->setCurrentIndex(1);
      } else {
         chat_->ui_->stackedWidgetMessages->setCurrentIndex(0);
      }

      // save draft
      if (!chat_->currentChat_.isEmpty()) {
         QString messageText = chat_->ui_->input_textEdit->toPlainText();
         chat_->draftMessages_[chat_->currentChat_] = messageText;
      }

      chat_->currentChat_ = roomId;
      chat_->setIsRoom(true);
      chat_->ui_->input_textEdit->setEnabled(!chat_->currentChat_.isEmpty());
      chat_->ui_->labelActiveChat->setText(QObject::tr("CHAT #") + chat_->currentChat_);
      chat_->ui_->textEditMessages->switchToChat(chat_->currentChat_, true);
      chat_->client_->retrieveRoomMessages(chat_->currentChat_);

      // load draft
      if (chat_->draftMessages_.contains(roomId)) {
         chat_->ui_->input_textEdit->setText(chat_->draftMessages_[roomId]);
      } else {
         chat_->ui_->input_textEdit->setText(QLatin1Literal(""));
      }
      chat_->ui_->input_textEdit->setFocus();
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
   , needsToStartFirstRoom_(false)
{
   ui_->setupUi(this);

   //Init UI and other stuff
   ui_->stackedWidget->setCurrentIndex(1); //Basically stackedWidget should be removed

   otcRequestViewModel_ = new OTCRequestViewModel(this);
   ui_->treeViewOTCRequests->setModel(otcRequestViewModel_);

   qRegisterMetaType<std::vector<std::string>>();

   connect(ui_->widgetCreateOTCRequest, &CreateOTCRequestWidget::RequestCreated, this, &ChatWidget::OnOTCRequestCreated);
   connect(ui_->widgetCreateOTCResponse, &CreateOTCResponseWidget::ResponseCreated, this, &ChatWidget::OnOTCResponseCreated);

   //widgetNegotiateRequest
   //widgetNegotiateResponse
}

ChatWidget::~ChatWidget() = default;

void ChatWidget::init(const std::shared_ptr<ConnectionManager>& connectionManager
                 , const std::shared_ptr<ApplicationSettings> &appSettings
                 , const std::shared_ptr<spdlog::logger>& logger)
{
   logger_ = logger;
   client_ = std::make_shared<ChatClient>(connectionManager, appSettings, logger);
   ui_->treeViewUsers->setModel(client_->getDataModel().get());
   ui_->treeViewUsers->expandAll();
   ui_->treeViewUsers->addWatcher(new LoggerWatcher());
   ui_->treeViewUsers->addWatcher(ui_->textEditMessages);
   ui_->treeViewUsers->addWatcher(this);
   ui_->treeViewUsers->setHandler(client_);
   ui_->textEditMessages->setHandler(client_);
   ui_->treeViewUsers->setActiveChatLabel(ui_->labelActiveChat);
   //ui_->chatSearchLineEdit->setActionsHandler(client_);

   connect(client_.get(), &ChatClient::LoginFailed, this, &ChatWidget::onLoginFailed);
   connect(client_.get(), &ChatClient::LoggedOut, this, &ChatWidget::onLoggedOut);
   connect(client_.get(), &ChatClient::SearchUserListReceived, this, &ChatWidget::onSearchUserListReceived);
   connect(ui_->input_textEdit, &BSChatInput::sendMessage, this, &ChatWidget::onSendButtonClicked);
   connect(ui_->chatSearchLineEdit, &ChatSearchLineEdit::textEdited, this, &ChatWidget::onSearchUserTextEdited);
   connect(ui_->treeViewUsers->model(), &QAbstractItemModel::modelReset, this, &ChatWidget::treeViewUsersModelReset);
   connect(ui_->treeViewUsers->model(), &QAbstractItemModel::rowsAboutToBeInserted, this, &ChatWidget::treeViewUsersModelRowsAboutToBeInserted);
   connect(ui_->treeViewUsers->model(), &QAbstractItemModel::rowsInserted, this, &ChatWidget::treeViewUsersModelRowsInserted);

//   connect(client_.get(), &ChatClient::SearchUserListReceived,
//           this, &ChatWidget::onSearchUserListReceived);
   //connect(ui_->chatSearchLineEdit, &ChatSearchLineEdit::returnPressed, this, &ChatWidget::onSearchUserReturnPressed);

   changeState(State::LoggedOut); //Initial state is LoggedOut
   initPopup();
}

void ChatWidget::onChatUserRemoved(const ChatUserDataPtr &chatUserDataPtr)
{
   if (currentChat_ == chatUserDataPtr->userId())
   {
      onUserClicked({});
   }
}

void ChatWidget::onAddChatRooms(const std::vector<std::shared_ptr<Chat::RoomData> >& roomList)
{
   if (roomList.size() > 0 && needsToStartFirstRoom_) {
     // ui_->treeViewUsers->selectFirstRoom();
      const auto &firstRoom = roomList.at(0);
      onRoomClicked(firstRoom->getId());
      needsToStartFirstRoom_ = false;
   }
}

void ChatWidget::onSearchUserListReceived(const std::vector<std::shared_ptr<Chat::UserData>>& users)
{
   if (users.size() > 0) {
      std::shared_ptr<Chat::UserData> firstUser = users.at(0);
      popup_->setUserID(firstUser->getUserId()); 
      popup_->setUserIsInContacts(client_->isFriend(firstUser->getUserId()));
   } else {
      popup_->setUserID(QString());
   }

   setPopupVisible(true);

   // hide popup after a few sec
   if (users.size() == 0) 
      popupVisibleTimer_->start(kShowEmptyFoundUserListTimeoutMs);
}

void ChatWidget::onUserClicked(const QString& userId)
{
   stateCurrent_->onUserClicked(userId);
}

void ChatWidget::onUsersDeleted(const std::vector<std::string> &users)
{
   stateCurrent_->onUsersDeleted(users);
}

void ChatWidget::treeViewUsersModelReset()
{
   // expand all by default
   ui_->treeViewUsers->expandAll();
}

void ChatWidget::treeViewUsersModelRowsAboutToBeInserted()
{
   // get all indexes
   QModelIndexList indexes = ui_->treeViewUsers->model()->match(ui_->treeViewUsers->model()->index(0,0),
                                                                Qt::DisplayRole,
                                                                QLatin1String("*"),
                                                                -1,
                                                                Qt::MatchWildcard|Qt::MatchRecursive);
   
   // save expanded indexes
   expandedIndexes_.clear();
   for (const auto &index : indexes)
      if (ui_->treeViewUsers->isExpanded(index))
         expandedIndexes_.insert(index);
}

void ChatWidget::treeViewUsersModelRowsInserted()
{
   // get all indexes
   QModelIndexList indexes = ui_->treeViewUsers->model()->match(ui_->treeViewUsers->model()->index(0,0),
                                                                Qt::DisplayRole,
                                                                QLatin1String("*"),
                                                                -1,
                                                                Qt::MatchWildcard|Qt::MatchRecursive);

    // restore expand and collaps states for all indexes
   for (const auto &index : indexes) {
      if (expandedIndexes_.find(index) != expandedIndexes_.end())
	      ui_->treeViewUsers->expand(index);
      else
         ui_->treeViewUsers->collapse(index);
   }
}

void ChatWidget::changeState(ChatWidget::State state)
{

   //Do not add any functionality here, except  states swapping

   if (!stateCurrent_) { //In case if we use change state in first time
      stateCurrent_ = std::make_shared<ChatWidgetStateLoggedOut>(this);
      stateCurrent_->onStateEnter();
   } else if (stateCurrent_->type() != state) {
      stateCurrent_->onStateExit();
      switch (state) {
      case State::LoggedIn:
         {
            stateCurrent_ = std::make_shared<ChatWidgetStateLoggedIn>(this);
         }
         break;
      case State::LoggedOut:
         {
            stateCurrent_ = std::make_shared<ChatWidgetStateLoggedOut>(this);
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

void ChatWidget::initPopup()
{   
   // create popup
   popup_ = new ChatSearchPopup(this);
   popup_->setGeometry(0, 0, ui_->chatSearchLineEdit->width(), static_cast<int>(ui_->chatSearchLineEdit->height() * 1.2));
   popup_->setVisible(false);
   connect(popup_, &ChatSearchPopup::sendFriendRequest, this, &ChatWidget::onSendFriendRequest);
   connect(popup_, &ChatSearchPopup::removeFriendRequest, this, &ChatWidget::onRemoveFriendRequest);
   qApp->installEventFilter(this);

   // insert popup under chat search line edit
   QVBoxLayout *boxLayout = qobject_cast<QVBoxLayout*>(ui_->chatSearchLineEdit->parentWidget()->layout());
   int index = boxLayout->indexOf(ui_->chatSearchLineEdit) + 1;
   boxLayout->insertWidget(index, popup_);

   // create spacer under popup
   chatUsersVerticalSpacer_ = new QSpacerItem(20, 40);
   boxLayout->insertSpacerItem(index+1, chatUsersVerticalSpacer_);

   // create timer
   popupVisibleTimer_ = new QTimer();
   popupVisibleTimer_->setSingleShot(true);
   connect(popupVisibleTimer_, &QTimer::timeout, [=]() {
      setPopupVisible(false);
   });
}

void ChatWidget::setPopupVisible(const bool &value)
{
   if (popup_ != NULL)
      popup_->setVisible(value);

   // resize spacer
   if (chatUsersVerticalSpacer_ != NULL) {
      if (value)
         chatUsersVerticalSpacer_->changeSize(20, 13);
      else
         chatUsersVerticalSpacer_->changeSize(20, 40);
      ui_->chatSearchLineEdit->parentWidget()->layout()->update();
   }

   if (popupVisibleTimer_ != NULL) {
      popupVisibleTimer_->stop();
   }
}

std::string ChatWidget::login(const std::string& email, const std::string& jwt)
{
   try {
      const auto userId = stateCurrent_->login(email, jwt);
      changeState(State::LoggedIn);
      needsToStartFirstRoom_ = true;
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
   return stateCurrent_->logout();
}

bool ChatWidget::hasUnreadMessages()
{
   return true;
}

void ChatWidget::switchToChat(const QString& chatId)
{
   onUserClicked(chatId);
}

void ChatWidget::onLoggedOut()
{
   stateCurrent_->onLoggedOut();
   emit LogOut();
}

void ChatWidget::onNewChatMessageTrayNotificationClicked(const QString &chatId)
{
   switchToChat(chatId);
}

void ChatWidget::onSearchUserTextEdited(const QString& text)
{
   QString userToAdd = ui_->chatSearchLineEdit->text();
   if (userToAdd.isEmpty() || userToAdd.length() < 3) {
      setPopupVisible(false);
      return;
   }

   QRegularExpression rx_email(QLatin1String(R"(^[a-z0-9._-]+@([a-z0-9-]+\.)+[a-z]+$)"), QRegularExpression::CaseInsensitiveOption);
   QRegularExpressionMatch match = rx_email.match(userToAdd);
   if (match.hasMatch()) {
      userToAdd = client_->deriveKey(userToAdd);
   } else if (UserHasher::KeyLength < userToAdd.length()) {
      return; //Initially max key is 12 symbols
   }
   client_->sendSearchUsersRequest(userToAdd);
}

bool ChatWidget::eventFilter(QObject *obj, QEvent *event)
{
   if ( popup_->isVisible() && event->type() == QEvent::MouseButtonRelease) {
      QPoint pos = popup_->mapFromGlobal(QCursor::pos());

      if (!popup_->rect().contains(pos))
         setPopupVisible(false);
   }

   return QWidget::eventFilter(obj, event);
}

void ChatWidget::onSendFriendRequest(const QString &userId)
{
   client_->sendFriendRequest(userId);
   setPopupVisible(false);
}

void ChatWidget::onRemoveFriendRequest(const QString &userId)
{
   client_->removeContact(userId);
   setPopupVisible(false);
}

void ChatWidget::onRoomClicked(const QString& roomId)
{
   stateCurrent_->onRoomClicked(roomId);
}

bool ChatWidget::isRoom()
{
   return isRoom_;
}

void ChatWidget::setIsRoom(bool isRoom)
{
   isRoom_ = isRoom;
}

void ChatWidget::onElementSelected(CategoryElement *element)
{
   if (element) {
      switch (element->getType()) {
         case TreeItem::NodeType::RoomsElement: {
            auto room = std::dynamic_pointer_cast<Chat::RoomData>(element->getDataObject());
            if (room) {
               setIsRoom(true);
               currentChat_ = room->getId();
            }
         }
         break;
         case TreeItem::NodeType::ContactsElement:{
            auto contact = std::dynamic_pointer_cast<Chat::ContactRecordData>(element->getDataObject());
            if (contact) {
               setIsRoom(false);
               currentChat_ = contact->getContactId();
            }
         }
         break;
         default:
            break;

      }
      if (currentChat_ == QLatin1Literal("otc_chat")) {
         ui_->stackedWidgetMessages->setCurrentIndex(1);
      } else {
         ui_->stackedWidgetMessages->setCurrentIndex(0);
      }
   }
}

void ChatWidget::onMessageChanged(std::shared_ptr<Chat::MessageData> message)
{
   qDebug() << __func__ << " " << QString::fromStdString(message->toJsonString());
}

void ChatWidget::onElementUpdated(CategoryElement *element)
{
   qDebug() << __func__ << " " << QString::fromStdString(element->getDataObject()->toJsonString());
}

void ChatWidget::OnOTCRequestCreated()
{
   auto side = ui_->widgetCreateOTCRequest->GetSide();
   auto range = ui_->widgetCreateOTCRequest->GetRange();

   DisplayOTCRequest(side, range);
}

void ChatWidget::DisplayOTCRequest(const bs::network::Side::Type& side, const bs::network::OTCRangeID& range)
{
   ui_->widgetCreateOTCResponse->SetSide(side);
   ui_->widgetCreateOTCResponse->SetRange(range);

   ui_->stackedWidgetOTC->setCurrentIndex(static_cast<int>(OTCPages::OTCCreateResponse));
}


void ChatWidget::OnOTCResponseCreated()
{
   auto priceRange = ui_->widgetCreateOTCResponse->GetResponsePriceRange();
   auto amountRange = ui_->widgetCreateOTCResponse->GetResponseQuantityRange();
   ui_->widgetNegotiateRequest->DisplayResponse(ui_->widgetCreateOTCRequest->GetSide(), priceRange, amountRange);

   ui_->stackedWidgetOTC->setCurrentIndex(static_cast<int>(OTCPages::OTCNegotiateRequest));
}
