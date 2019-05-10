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
#include "NotificationCenter.h"
#include "ZMQ_BIP15X_DataConnection.h"


Q_DECLARE_METATYPE(std::vector<std::string>)


enum class OTCPages : int
{
   OTCLoginRequiredShieldPage = 0,
   OTCGeneralRoomShieldPage,
   OTCCreateRequestPage,
   OTCCreateResponsePage,
   OTCNegotiateRequestPage,
   OTCNegotiateResponsePage
};

constexpr int kShowEmptyFoundUserListTimeoutMs = 3000;

bool IsOTCChatRoom(const QString& chatRoom)
{
   static const QString targetRoomName = Chat::OTCRoomKey;
   return chatRoom == targetRoomName;
}

bool IsGlobalChatRoom(const QString& chatRoom)
{
   static const QString targetRoomName = Chat::GlobalRoomKey;
   return chatRoom == targetRoomName;
}


class ChatWidgetState {
public:
    virtual void onStateEnter() {} //Do something special on state appears, by default nothing
    virtual void onStateExit() {} //Do something special on state about to gone, by default nothing

public:

   explicit ChatWidgetState(ChatWidget* chat, ChatWidget::State type) : chat_(chat), type_(type) {}
   virtual ~ChatWidgetState() = default;

   virtual std::string login(const std::string& email, const std::string& jwt
      , const ZmqBIP15XDataConnection::cbNewKey &) = 0;
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

      chat_->SetLoggedOutOTCState();

      // hide tab icon for unread messages
      NotificationCenter::notify(bs::ui::NotifyType::UpdateUnreadMessage, {});
   }

   std::string login(const std::string& email, const std::string& jwt
      , const ZmqBIP15XDataConnection::cbNewKey &cb) override {
      chat_->logger_->debug("Set user name {}", email);
      const auto userId = chat_->client_->loginToServer(email, jwt, cb);
      chat_->ui_->textEditMessages->setOwnUserId(userId);
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
      chat_->ui_->treeViewUsers->expandAll();
      chat_->ui_->labelUserName->setText(chat_->client_->getUserId());

      chat_->SetOTCLoggedInState();
   }

   void onStateExit() override {
      chat_->onUserClicked({});
   }

   std::string login(const std::string& /*email*/, const std::string& /*jwt*/
      , const ZmqBIP15XDataConnection::cbNewKey &) override {
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
      if (IsOTCChatRoom(roomId)) {
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
   auto model = client_->getDataModel();
   model->setNewMessageMonitor(this);
   ui_->treeViewUsers->setModel(model.get());
//   ui_->treeViewUsers->expandAll();
   ui_->treeViewUsers->addWatcher(new LoggerWatcher());
   ui_->treeViewUsers->addWatcher(ui_->textEditMessages);
   ui_->treeViewUsers->addWatcher(this);
   ui_->treeViewUsers->setHandler(client_);
   ui_->textEditMessages->setHandler(client_);
   ui_->textEditMessages->setMessageReadHandler(client_);

   ui_->treeViewUsers->setActiveChatLabel(ui_->labelActiveChat);
   //ui_->chatSearchLineEdit->setActionsHandler(client_);

   connect(client_.get(), &ChatClient::LoginFailed, this, &ChatWidget::onLoginFailed);
   connect(client_.get(), &ChatClient::LoggedOut, this, &ChatWidget::onLoggedOut);
   connect(client_.get(), &ChatClient::SearchUserListReceived, this, &ChatWidget::onSearchUserListReceived);
   connect(client_.get(), &ChatClient::ConnectedToServer, this, &ChatWidget::onConnectedToServer);
   connect(client_.get(), &ChatClient::ContactRequestAccepted, this, &ChatWidget::onContactRequestAccepted);
   connect(client_.get(), &ChatClient::NewContactRequest, this, [=] (const QString &userId) {
            NotificationCenter::notify(bs::ui::NotifyType::FriendRequest, {userId});
   });
   connect(ui_->input_textEdit, &BSChatInput::sendMessage, this, &ChatWidget::onSendButtonClicked);
   connect(ui_->chatSearchLineEdit, &ChatSearchLineEdit::textEdited, this, &ChatWidget::onSearchUserTextEdited);

//   connect(client_.get(), &ChatClient::SearchUserListReceived,
//           this, &ChatWidget::onSearchUserListReceived);
   //connect(ui_->chatSearchLineEdit, &ChatSearchLineEdit::returnPressed, this, &ChatWidget::onSearchUserReturnPressed);

   changeState(State::LoggedOut); //Initial state is LoggedOut
   initPopup();
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

std::string ChatWidget::login(const std::string& email, const std::string& jwt
   , const ZmqBIP15XDataConnection::cbNewKey &cb)
{
   try {
      const auto userId = stateCurrent_->login(email, jwt, cb);
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

void ChatWidget::onConnectedToServer()
{
   changeState(State::LoggedIn);
   connect(ui_->treeViewUsers->model(), &QAbstractItemModel::dataChanged, this, &ChatWidget::selectFirstRoom);
}

void ChatWidget::onContactRequestAccepted(const QString &userId)
{
   // select user in chat tree
   stateCurrent_->onUserClicked(userId);

   // highlight user in chat tree
   QModelIndexList indexes = ui_->treeViewUsers->model()->match(ui_->treeViewUsers->model()->index(0,0),
                                                                Qt::DisplayRole,
                                                                QLatin1String("*"),
                                                                -1,
                                                                Qt::MatchWildcard|Qt::MatchRecursive);

   for (auto index : indexes) {
      if (index.data(ChatClientDataModel::Role::ItemTypeRole).value<TreeItem::NodeType>() == TreeItem::NodeType::ContactsElement) {
         if (userId == index.data(ChatClientDataModel::Role::ContactIdRole).toString() ) {
            ui_->treeViewUsers->selectionModel()->select(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
            break;
         }
      }
   }
}

bool ChatWidget::eventFilter(QObject *obj, QEvent *event)
{
   if ( popup_->isVisible() && event->type() == QEvent::MouseButtonRelease) {
      QPoint pos = popup_->mapFromGlobal(QCursor::pos());

      if (!popup_->rect().contains(pos))
         setPopupVisible(false);
   }

   if (event->type() == QEvent::WindowActivate) {
      // hide tab icon on window activate event
      NotificationCenter::notify(bs::ui::NotifyType::UpdateUnreadMessage, {});
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

      if (IsOTCChatRoom(currentChat_)) {
         ui_->stackedWidgetMessages->setCurrentIndex(1);
         OTCSwitchToCommonRoom();
      } else {
         ui_->stackedWidgetMessages->setCurrentIndex(0);
         if (IsGlobalChatRoom(currentChat_)) {
            OTCSwitchToGlobalRoom();
         } else {
            OTCSwitchToDMRoom();
         }
      }
   }
}

void ChatWidget::onMessageChanged(std::shared_ptr<Chat::MessageData> message)
{
#if 0
   qDebug() << __func__ << " " << QString::fromStdString(message->toJsonString());
#endif
}

void ChatWidget::onElementUpdated(CategoryElement *element)
{
#if 0
   qDebug() << __func__ << " " << QString::fromStdString(element->getDataObject()->toJsonString());
#endif
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

   ui_->stackedWidgetOTC->setCurrentIndex(static_cast<int>(OTCPages::OTCCreateResponsePage));
}


void ChatWidget::OnOTCResponseCreated()
{
   auto priceRange = ui_->widgetCreateOTCResponse->GetResponsePriceRange();
   auto amountRange = ui_->widgetCreateOTCResponse->GetResponseQuantityRange();
   ui_->widgetNegotiateRequest->DisplayResponse(ui_->widgetCreateOTCRequest->GetSide(), priceRange, amountRange);

   ui_->stackedWidgetOTC->setCurrentIndex(static_cast<int>(OTCPages::OTCNegotiateRequestPage));
}


void ChatWidget::SetOTCLoggedInState()
{
   OTCSwitchToGlobalRoom();
}

void ChatWidget::SetLoggedOutOTCState()
{
   ui_->stackedWidgetOTC->setCurrentIndex(static_cast<int>(OTCPages::OTCLoginRequiredShieldPage));
}

void ChatWidget::OTCSwitchToCommonRoom()
{
   ui_->stackedWidgetOTC->setCurrentIndex(static_cast<int>(OTCPages::OTCCreateRequestPage));
}

void ChatWidget::OTCSwitchToDMRoom()
{
   ui_->stackedWidgetOTC->setCurrentIndex(static_cast<int>(OTCPages::OTCCreateRequestPage));
}

void ChatWidget::OTCSwitchToGlobalRoom()
{
   ui_->stackedWidgetOTC->setCurrentIndex(static_cast<int>(OTCPages::OTCGeneralRoomShieldPage));
}

void ChatWidget::onNewMessagePresent(const bool isNewMessagePresented, std::shared_ptr<Chat::MessageData> message)
{
   qDebug() << "New Message: " << (isNewMessagePresented?"TRUE":"FALSE");

   // show notification of new message in tray icon
   if (isNewMessagePresented) {

      // don't show notification for global chat
      if (message && !IsGlobalChatRoom(message->receiverId())) {

         auto messageText = message->messageData();
         const int maxMessageLength = 20;

         if (messageText.length() > maxMessageLength) {
            messageText = messageText.mid(0, maxMessageLength) + QLatin1String("...");
         }

         NotificationCenter::notify(bs::ui::NotifyType::UpdateUnreadMessage,
                                   {message->senderId(),
                                    messageText});
      }
   }
}

void ChatWidget::selectFirstRoom()
{
   QModelIndexList indexes = ui_->treeViewUsers->model()->match(ui_->treeViewUsers->model()->index(0,0),
                                                               Qt::DisplayRole,
                                                               QLatin1String("*"),
                                                               -1,
                                                               Qt::MatchWildcard|Qt::MatchRecursive);
      
   // highlight first room
   for (auto index : indexes) {
      if (index.data(ChatClientDataModel::Role::ItemTypeRole).value<TreeItem::NodeType>() == TreeItem::NodeType::RoomsElement) {
         if (index.data(ChatClientDataModel::Role::RoomIdRole).toString() == Chat::GlobalRoomKey) {
            onRoomClicked(Chat::GlobalRoomKey);
            ui_->treeViewUsers->selectionModel()->select(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
            disconnect(ui_->treeViewUsers->model(), &QAbstractItemModel::dataChanged, this, &ChatWidget::selectFirstRoom);
            break;
         }
      }
   }
}
