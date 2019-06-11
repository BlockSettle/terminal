#include "ChatWidget.h"
#include "ui_ChatWidget.h"

#include "ApplicationSettings.h"
#include "ChatClient.h"
#include "ChatClientDataModel.h"
#include "NotificationCenter.h"
#include "OTCRequestViewModel.h"
#include "UserHasher.h"
#include "ZMQ_BIP15X_DataConnection.h"
#include "ChatTreeModelWrapper.h"
#include "UserSearchModel.h"
#include "CelerClient.h"
#include "ChatSearchListViewItemStyle.h"

#include <QApplication>
#include <QMouseEvent>
#include <QObject>
#include <QScrollBar>
#include <QClipboard>
#include <QMimeData>

#include <QDebug>

#include <thread>
#include <spdlog/spdlog.h>

Q_DECLARE_METATYPE(std::vector<std::string>)


enum class OTCPages : int
{
   OTCLoginRequiredShieldPage = 0,
   OTCGeneralRoomShieldPage,
   OTCCreateRequestPage,
   OTCPullOwnOTCRequestPage,
   OTCCreateResponsePage,
   OTCNegotiateRequestPage,
   OTCNegotiateResponsePage,
   OTCParticipantShieldPage,
   OTCContactShieldPage,
   OTCContactNetStatusShieldPage
};

const QRegularExpression kRxEmail(QStringLiteral(R"(^[a-z0-9._-]+@([a-z0-9-]+\.)+[a-z]+$)"),
                                  QRegularExpression::CaseInsensitiveOption);

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
      chat_->ui_->searchWidget->clearLineEdit();
      chat_->ui_->searchWidget->setLineEditEnabled(false);
      chat_->ui_->labelUserName->setText(QLatin1String("offline"));

      chat_->SetLoggedOutOTCState();

      NotificationCenter::notify(bs::ui::NotifyType::LogOut, {});
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
      chat_->ui_->searchWidget->setLineEditEnabled(true);
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

   void onMessagesUpdated() override {
      QScrollBar *bar = chat_->ui_->textEditMessages->verticalScrollBar();
      bar->setValue(bar->maximum());
   }

   void onLoginFailed() override {
      chat_->changeState(ChatWidget::LoggedOut);
   }

   void onUsersDeleted(const std::vector<std::string> &/*users*/) override
   {}
};

ChatWidget::ChatWidget(QWidget *parent)
   : QWidget(parent)
   , ui_(new Ui::ChatWidget)
   , needsToStartFirstRoom_(false)
{
   ui_->setupUi(this);

#ifndef Q_OS_WIN
   ui_->timeLabel->setMinimumSize(ui_->timeLabel->property("minimumSizeLinux").toSize());
#endif

   ui_->textEditMessages->setColumnsWidth(ui_->timeLabel->minimumWidth(),
                                          ui_->iconLabel->minimumWidth(),
                                          ui_->userLabel->minimumWidth(),
                                          ui_->messageLabel->minimumWidth());

   //Init UI and other stuff
   ui_->stackedWidget->setCurrentIndex(1); //Basically stackedWidget should be removed

   otcRequestViewModel_ = new OTCRequestViewModel(this);
   ui_->treeViewOTCRequests->setModel(otcRequestViewModel_);

   qRegisterMetaType<std::vector<std::string>>();

   connect(ui_->widgetCreateOTCRequest, &CreateOTCRequestWidget::RequestCreated, this, &ChatWidget::OnOTCRequestCreated);
   connect(ui_->widgetCreateOTCResponse, &CreateOTCResponseWidget::ResponseCreated, this, &ChatWidget::OnCreateResponse);

   connect(ui_->widgetPullOwnOTCRequest, &PullOwnOTCRequestWidget::PullOTCRequested, this, &ChatWidget::OnCancelCurrentTrading);
   connect(ui_->widgetCreateOTCResponse, &CreateOTCResponseWidget::ResponseRejected, this, &ChatWidget::OnCancelCurrentTrading);

   connect(ui_->widgetNegotiateRequest, &OTCNegotiationRequestWidget::TradeUpdated, this, &ChatWidget::OnUpdateTradeRequestor);
   connect(ui_->widgetNegotiateRequest, &OTCNegotiationRequestWidget::TradeAccepted, this, &ChatWidget::OnAcceptTradeRequestor);
   connect(ui_->widgetNegotiateRequest, &OTCNegotiationRequestWidget::TradeRejected, this, &ChatWidget::OnCancelCurrentTrading);

   connect(ui_->widgetNegotiateResponse, &OTCNegotiationResponseWidget::TradeUpdated, this, &ChatWidget::OnUpdateTradeResponder);
   connect(ui_->widgetNegotiateResponse, &OTCNegotiationResponseWidget::TradeAccepted, this, &ChatWidget::OnAcceptTradeResponder);
   connect(ui_->widgetNegotiateResponse, &OTCNegotiationResponseWidget::TradeRejected, this, &ChatWidget::OnCancelCurrentTrading);
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
   auto proxyModel = client_->getProxyModel();
   ui_->treeViewUsers->setModel(proxyModel.get());
   ui_->treeViewUsers->setSortingEnabled(true);
   ui_->treeViewUsers->sortByColumn(0, Qt::AscendingOrder);
   connect(proxyModel.get(), &ChatTreeModelWrapper::treeUpdated,
           ui_->treeViewUsers, &QTreeView::expandAll);
//   ui_->treeViewUsers->expandAll();
   ui_->treeViewUsers->addWatcher(ui_->textEditMessages);
   ui_->treeViewUsers->addWatcher(this);
   ui_->treeViewUsers->setHandler(client_);
   ui_->textEditMessages->setHandler(client_);
   ui_->textEditMessages->setMessageReadHandler(client_);
   ui_->textEditMessages->setClient(client_);
   ui_->input_textEdit->setAcceptRichText(false);

   ui_->treeViewUsers->setActiveChatLabel(ui_->labelActiveChat);
   //ui_->chatSearchLineEdit->setActionsHandler(client_);

   connect(client_.get(), &ChatClient::LoginFailed, this, &ChatWidget::onLoginFailed);
   connect(client_.get(), &ChatClient::LoggedOut, this, &ChatWidget::onLoggedOut);
   connect(client_.get(), &ChatClient::SearchUserListReceived, this, &ChatWidget::onSearchUserListReceived);
   connect(client_.get(), &ChatClient::ConnectedToServer, this, &ChatWidget::onConnectedToServer);
   connect(client_.get(), &ChatClient::ContactRequestAccepted, this, &ChatWidget::onContactRequestAccepted);
   connect(client_.get(), &ChatClient::RoomsInserted, this, &ChatWidget::selectGlobalRoom);
   connect(client_.get(), &ChatClient::NewContactRequest, this, [=] (const QString &userId) {
            NotificationCenter::notify(bs::ui::NotifyType::FriendRequest, {userId});
   });
   connect(ui_->input_textEdit, &BSChatInput::sendMessage, this, &ChatWidget::onSendButtonClicked);
   connect(ui_->input_textEdit, &BSChatInput::selectionChanged, this, &ChatWidget::onBSChatInputSelectionChanged);
   connect(ui_->searchWidget, &SearchWidget::searchUserTextEdited, this, &ChatWidget::onSearchUserTextEdited);
   connect(ui_->textEditMessages, &QTextEdit::selectionChanged, this, &ChatWidget::onChatMessagesSelectionChanged);

//   connect(client_.get(), &ChatClient::SearchUserListReceived,
//           this, &ChatWidget::onSearchUserListReceived);
   //connect(ui_->chatSearchLineEdit, &ChatSearchLineEdit::returnPressed, this, &ChatWidget::onSearchUserReturnPressed);

   connect(ui_->treeViewOTCRequests->selectionModel(), &QItemSelectionModel::selectionChanged
      , this, &ChatWidget::OnOTCSelectionChanged);

   changeState(State::LoggedOut); //Initial state is LoggedOut
   initSearchWidget();
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

void ChatWidget::onSearchUserListReceived(const std::vector<std::shared_ptr<Chat::UserData>>& users, bool emailEntered)
{
   std::vector<UserSearchModel::UserInfo> userInfoList;
   QString searchText = ui_->searchWidget->searchText();
   bool isEmail = kRxEmail.match(searchText).hasMatch();
   QString hash = client_->deriveKey(searchText);
   for (const auto &user : users) {
      if (user) {
         QString userId = user->getUserId();
         if (isEmail && userId != hash) {
            continue;
         }
         auto status = UserSearchModel::UserStatus::ContactUnknown;
         auto contact = client_->getContact(userId);
         if (contact.isValid()) {
            auto contactStatus = contact.getContactStatus();
            switch (contactStatus) {
            case Chat::ContactStatus::Accepted:
               status = UserSearchModel::UserStatus::ContactAccepted;
               break;
            case Chat::ContactStatus::Incoming:
               status = UserSearchModel::UserStatus::ContactPendingIncoming;
               break;
            case Chat::ContactStatus::Outgoing:
               status = UserSearchModel::UserStatus::ContactPendingOutgoing;
               break;
            case Chat::ContactStatus::Rejected:
               status = UserSearchModel::UserStatus::ContactRejected;
               break;
            /*default:
               break;*/
            }
         }
         userInfoList.emplace_back(userId, status);
      }
   }
   client_->getUserSearchModel()->setUsers(userInfoList);

   bool visible = true;
   if (isEmail) {
      visible = emailEntered || !userInfoList.empty();
      if (visible) {
         ui_->searchWidget->clearSearchLineOnNextInput();
      }
   } else {
      visible = !userInfoList.empty();
   }
   ui_->searchWidget->setListVisible(visible);

   // hide popup after a few sec
   if (visible && userInfoList.empty()) {
      ui_->searchWidget->startListAutoHide();
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

void ChatWidget::initSearchWidget()
{
   ui_->searchWidget->init(client_);
   ui_->searchWidget->setSearchModel(client_->getUserSearchModel());
   client_->getUserSearchModel()->setItemStyle(std::make_shared<ChatSearchListViewItemStyle>());
   connect(ui_->searchWidget, &SearchWidget::addFriendRequied,
           this, &ChatWidget::onSendFriendRequest);
   connect(ui_->searchWidget, &SearchWidget::removeFriendRequired,
           this, &ChatWidget::onRemoveFriendRequest);
}

void ChatWidget::onSendButtonClicked()
{
   return stateCurrent_->onSendButtonClicked();
}

void ChatWidget::onMessagesUpdated()
{
   return stateCurrent_->onMessagesUpdated();
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

void ChatWidget::setCelerClient(std::shared_ptr<CelerClient> celerClient)
{
   celerClient_ = celerClient;
}

void ChatWidget::updateChat(const bool &isChatTab)
{
   isChatTab_ = isChatTab;

   ui_->textEditMessages->setIsChatTab(isChatTab_);

   if (isChatTab_) {
      ui_->treeViewUsers->updateCurrentChat();
   }
}

void ChatWidget::onLoggedOut()
{
   stateCurrent_->onLoggedOut();
   emit LogOut();
}

void ChatWidget::onNewChatMessageTrayNotificationClicked(const QString &userId)
{
   ui_->treeViewUsers->setCurrentUserChat(userId);
}

void ChatWidget::onSearchUserTextEdited(const QString& /*text*/)
{
   QString userToAdd = ui_->searchWidget->searchText();
   if (userToAdd.isEmpty() || userToAdd.length() < 3) {
      ui_->searchWidget->setListVisible(false);
      client_->getUserSearchModel()->setUsers({});
      return;
   }

   QRegularExpressionMatch match = kRxEmail.match(userToAdd);
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
}

void ChatWidget::onContactRequestAccepted(const QString &userId)
{
   ui_->treeViewUsers->setCurrentUserChat(userId);
}

bool ChatWidget::eventFilter(QObject *sender, QEvent *event)
{
   if (event->type() == QEvent::WindowActivate) {
      // hide tab icon on window activate event
      NotificationCenter::notify(bs::ui::NotifyType::UpdateUnreadMessage, {});

      if (isChatTab_) {
         ui_->treeViewUsers->updateCurrentChat();
      }
   }

   // copy selected messages by keyboard shortcut
   if (event->type() == QEvent::KeyPress && sender == ui_->input_textEdit) {
      QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);

      // handle ctrl+c (cmd+c on macOS)
      if(keyEvent->key() == Qt::Key_C && keyEvent->modifiers().testFlag(Qt::ControlModifier)) {
         if (ui_->textEditMessages->textCursor().hasSelection() && isChatMessagesSelected_) {
            QApplication::clipboard()->setText(ui_->textEditMessages->getFormattedTextFromSelection());
            return true;
         }
      }
   }

   return QWidget::eventFilter(sender, event);
}

void ChatWidget::onSendFriendRequest(const QString &userId)
{
   client_->sendFriendRequest(userId);
   ui_->searchWidget->setListVisible(false);
}

void ChatWidget::onRemoveFriendRequest(const QString &userId)
{
   client_->removeContact(userId);
   ui_->searchWidget->setListVisible(false);
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
         case ChatUIDefinitions::ChatTreeNodeType::RoomsElement: {
            auto room = std::dynamic_pointer_cast<Chat::RoomData>(element->getDataObject());
            if (room) {
               setIsRoom(true);
               currentChat_ = room->getId();
               OTCSwitchToRoom(room);
            }
         }
         break;
         case ChatUIDefinitions::ChatTreeNodeType::ContactsElement:{
            auto contact = std::dynamic_pointer_cast<Chat::ContactRecordData>(element->getDataObject());
            if (contact) {
               setIsRoom(false);
               currentChat_ = contact->getContactId();
               ChatContactElement * cElement = dynamic_cast<ChatContactElement*>(element);
               OTCSwitchToContact(contact, cElement->getOnlineStatus()
                                  == ChatContactElement::OnlineStatus::Online);
            }
         }
         break;
         // XXXOTC
         // case ChatUIDefinitions::ChatTreeNodeType::OTCSentResponsesElement:{
         //    ui_->stackedWidgetMessages->setCurrentIndex(0);
         //    auto response = std::dynamic_pointer_cast<Chat::OTCResponseData>(element->getDataObject());
         //    if (response) {
         //       setIsRoom(false);
         //       currentChat_ = QString::fromStdString(response->serverResponseId());
         //    }
         // }
         // break;
         // case ChatUIDefinitions::ChatTreeNodeType::OTCReceivedResponsesElement:{
         //    ui_->stackedWidgetMessages->setCurrentIndex(0);
         //    auto response = std::dynamic_pointer_cast<Chat::OTCResponseData>(element->getDataObject());
         //    if (response) {
         //       setIsRoom(false);
         //       currentChat_ = QString::fromStdString(response->serverResponseId());
         //       OTCSwitchToResponse(response);
         //    }
         // }
         // break;
         default:
            break;

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
   if (element) {
      switch (element->getType()) {
         case ChatUIDefinitions::ChatTreeNodeType::RoomsElement: {
            auto room = std::dynamic_pointer_cast<Chat::RoomData>(element->getDataObject());
            if (room && currentChat_ == room->getId()) {
               OTCSwitchToRoom(room);
            }
         }
         break;
         case ChatUIDefinitions::ChatTreeNodeType::ContactsElement:{
            auto contact = std::dynamic_pointer_cast<Chat::ContactRecordData>(element->getDataObject());
            if (contact && currentChat_ == contact->getContactId()) {
                ChatContactElement * cElement = dynamic_cast<ChatContactElement*>(element);
               OTCSwitchToContact(contact, cElement->getOnlineStatus()
                                  == ChatContactElement::OnlineStatus::Online);
            }
         }
         break;
         default:
            break;
      }
   }
#if 0
   qDebug() << __func__ << " " << QString::fromStdString(element->getDataObject()->toJsonString());
#endif
}

void ChatWidget::OnOTCRequestCreated()
{
   const auto side = ui_->widgetCreateOTCRequest->GetSide();
   const auto range = ui_->widgetCreateOTCRequest->GetRange();

   auto otcRequest = bs::network::OTCRequest{side, range};

   if (currentChat_ == Chat::OTCRoomKey) {
      // XXXOTC
      // submit request to OTC room
   } else {
      if (!client_->SubmitPrivateOTCRequest(otcRequest, currentChat_)) {
         logger_->error("[ChatWidget::OnOTCRequestCreated] failed to submit"
                        " OTC request to {}", currentChat_.toStdString());
         return;
      }
   }
}

void ChatWidget::OnCreateResponse()
{
   if (currentChat_ == Chat::OTCRoomKey) {
      // XXXOTC
      // submit cancel to room
   } else {
      auto response = ui_->widgetCreateOTCResponse->GetCurrentOTCResponse();
      if (!client_->SubmitPrivateOTCResponse(response, currentChat_)) {
         logger_->error("[ChatWidget::OnCancelCurrentTrading] failed to submit response");
      }
   }
}

void ChatWidget::OnCancelCurrentTrading()
{
   if (currentChat_ == Chat::OTCRoomKey) {
      // XXXOTC
      // submit cancel to room
   } else {
      if (!client_->SubmitPrivateCancel(currentChat_)) {
         logger_->error("[ChatWidget::OnCancelCurrentTrading] failed to submit cancel");
      }
   }
}

void ChatWidget::OnUpdateTradeRequestor()
{
   auto update = ui_->widgetNegotiateRequest->GetUpdate();
   if (!client_->SubmitPrivateUpdate(update, currentChat_)) {
      logger_->error("[ChatWidget::OnUpdateTradeRequestor] failed to submit update");
   }
}

void ChatWidget::OnAcceptTradeRequestor()
{}

void ChatWidget::OnUpdateTradeResponder()
{
   auto update = ui_->widgetNegotiateResponse->GetUpdate();
   if (!client_->SubmitPrivateUpdate(update, currentChat_)) {
      logger_->error("[ChatWidget::OnCancelCurrentTrading] failed to submit update");
   }
}

void ChatWidget::OnAcceptTradeResponder()
{}

void ChatWidget::SetOTCLoggedInState()
{
   OTCSwitchToGlobalRoom();
}

void ChatWidget::SetLoggedOutOTCState()
{
   ui_->stackedWidgetOTC->setCurrentIndex(static_cast<int>(OTCPages::OTCLoginRequiredShieldPage));
}

bool ChatWidget::TradingAvailableForUser() const
{
   return celerClient_
      && (   celerClient_->celerUserType() == CelerClient::CelerUserType::Dealing
          || celerClient_->celerUserType() == CelerClient::CelerUserType::Trading);
}

void ChatWidget::OTCSwitchToCommonRoom()
{
   const auto currentSeletion = ui_->treeViewOTCRequests->selectionModel()->selection();
   if (currentSeletion.indexes().isEmpty()) {
      // OTC available only for trading and dealing participants
      if (TradingAvailableForUser()) {
         DisplayCorrespondingOTCRequestWidget();
      }
      else {
         ui_->stackedWidgetOTC->setCurrentIndex(static_cast<int>(OTCPages::OTCParticipantShieldPage));
      }
   }
   else {
      ui_->treeViewOTCRequests->selectionModel()->clearSelection();
   }
}

void ChatWidget::OTCSwitchToGlobalRoom()
{
   ui_->stackedWidgetOTC->setCurrentIndex(static_cast<int>(OTCPages::OTCGeneralRoomShieldPage));
}

void ChatWidget::OTCSwitchToRoom(std::shared_ptr<Chat::RoomData>& room)
{
   if (room->isTradingAvailable()) {
      ui_->stackedWidgetMessages->setCurrentIndex(1);
      OTCSwitchToCommonRoom();
   } else {
      ui_->stackedWidgetMessages->setCurrentIndex(0);
      OTCSwitchToGlobalRoom();
   }
}

void ChatWidget::OTCSwitchToContact(std::shared_ptr<Chat::ContactRecordData>& contact,
                                    bool onlineStatus)
{
   ui_->stackedWidgetMessages->setCurrentIndex(0);

   if (!TradingAvailableForUser()) {
      ui_->stackedWidgetOTC->setCurrentIndex(static_cast<int>(OTCPages::OTCParticipantShieldPage));
      return;
   }

   if (contact->getContactStatus() == Chat::ContactStatus::Accepted) {
      auto cNode = client_->getDataModel()->findContactNode(contact->getContactId().toStdString());
      if (onlineStatus) {
         if (cNode->OTCTradingStarted()) {
            if (cNode->isOTCRequestor()) {
               if (cNode->haveUpdates()) {
                  // display requester update from update
                  ui_->widgetNegotiateRequest->SetUpdateData(cNode->getLastOTCUpdate(), cNode->getOTCResponse());
                  ui_->stackedWidgetOTC->setCurrentIndex(static_cast<int>(OTCPages::OTCNegotiateRequestPage));
               } else {
                  if (cNode->haveResponse()) {
                     // display requester update from response
                     ui_->widgetNegotiateRequest->SetResponseData(cNode->getOTCResponse());
                     ui_->stackedWidgetOTC->setCurrentIndex(static_cast<int>(OTCPages::OTCNegotiateRequestPage));
                  } else {
                     // display own request for pull
                     ui_->widgetPullOwnOTCRequest->setRequestData(cNode->getOTCRequest());
                     ui_->stackedWidgetOTC->setCurrentIndex(static_cast<int>(OTCPages::OTCPullOwnOTCRequestPage));
                  }
               }
            } else {
               if (cNode->haveUpdates()) {
                  // display responder update from update
                  ui_->widgetNegotiateResponse->SetUpdateData(cNode->getLastOTCUpdate(), cNode->getOTCResponse());
                  ui_->stackedWidgetOTC->setCurrentIndex(static_cast<int>(OTCPages::OTCNegotiateResponsePage));
               } else {
                  if (cNode->haveResponse()) {
                     // display pull own response
                     ui_->widgetCreateOTCResponse->SetSubmittedResponse(cNode->getOTCResponse(), cNode->getOTCRequest());
                     ui_->stackedWidgetOTC->setCurrentIndex(static_cast<int>(OTCPages::OTCCreateResponsePage));
                  } else {
                     // display response widget
                     ui_->widgetCreateOTCResponse->SetRequestToRespond(cNode->getOTCRequest());
                     ui_->stackedWidgetOTC->setCurrentIndex(static_cast<int>(OTCPages::OTCCreateResponsePage));
                  }
               }
            }
         } else {
            DisplayCreateOTCWidget();
         }
      } else {
         ui_->stackedWidgetOTC->setCurrentIndex(static_cast<int>(OTCPages::OTCContactNetStatusShieldPage));
         cNode->cleanupTrading();
      }
   } else {
      ui_->stackedWidgetOTC->setCurrentIndex(static_cast<int>(OTCPages::OTCContactShieldPage));
   }
}

void ChatWidget::OTCSwitchToResponse(std::shared_ptr<Chat::OTCResponseData> &response)
{
   ui_->stackedWidgetMessages->setCurrentIndex(0);
}

void ChatWidget::UpdateOTCRoomWidgetIfRequired()
{
   if (IsOTCChatSelected()) {
      const auto currentSeletion = ui_->treeViewOTCRequests->selectionModel()->selection();
      if (currentSeletion.indexes().isEmpty()) {
         DisplayCorrespondingOTCRequestWidget();
      }
   }
}

// OTC request selected in OTC room
void ChatWidget::OnOTCSelectionChanged(const QItemSelection &selected, const QItemSelection &)
{
   // if (!selected.indexes().isEmpty()) {
   //    const auto otc = otcRequestViewModel_->GetOTCRequest(selected.indexes()[0]);

   //    if (otc == nullptr) {
   //       logger_->error("[ChatWidget::OnOTCSelectionChanged] can't get selected OTC");
   //       return;
   //    }

   //    if (IsOwnOTCId(otc->serverRequestId())) {
   //       // display request that could be pulled
   //       ui_->widgetPullOwnOTCRequest->DisplayActiveOTC(otc);
   //       ui_->stackedWidgetOTC->setCurrentIndex(static_cast<int>(OTCPages::OTCPullOwnOTCRequestPage));
   //    } else {
   //       // display create OTC response
   //       // NOTE: do we need to switch to channel if we already replied to this OTC?
   //       // what if we already replied to this?
   //       ui_->widgetCreateOTCResponse->SetActiveOTCRequest(otc);
   //       ui_->stackedWidgetOTC->setCurrentIndex(static_cast<int>(OTCPages::OTCCreateResponsePage));
   //    }
   // } else {
   //    DisplayCorrespondingOTCRequestWidget();
   // }
}

void ChatWidget::DisplayCreateOTCWidget()
{
   ui_->stackedWidgetOTC->setCurrentIndex(static_cast<int>(OTCPages::OTCCreateRequestPage));
}

void ChatWidget::DisplayOwnLiveOTC()
{
   //ui_->widgetPullOwnOTCRequest->DisplayActiveOTC(ownActiveOTC_);
   ui_->stackedWidgetOTC->setCurrentIndex(static_cast<int>(OTCPages::OTCPullOwnOTCRequestPage));
}

void ChatWidget::DisplayOwnSubmittedOTC()
{
   // ui_->widgetPullOwnOTCRequest->DisplaySubmittedOTC(submittedOtc_);
   ui_->stackedWidgetOTC->setCurrentIndex(static_cast<int>(OTCPages::OTCPullOwnOTCRequestPage));
}

void ChatWidget::DisplayCorrespondingOTCRequestWidget()
{
   if (IsOTCRequestSubmitted()) {
      if (IsOTCRequestAccepted()) {
         DisplayOwnLiveOTC();
      } else {
         DisplayOwnSubmittedOTC();
      }
   } else {
      DisplayCreateOTCWidget();
   }
}

bool ChatWidget::IsOTCRequestSubmitted() const
{
   // XXXPTC
   // return otcSubmitted_;
   return false;
}

bool ChatWidget::IsOTCRequestAccepted() const
{
   return false;
   //XXXOTC
   //return otcAccepted_;
}

bool ChatWidget::IsOTCChatSelected() const
{
   return IsOTCChatRoom(currentChat_);
}

void ChatWidget::onNewMessagesPresent(std::map<QString, std::shared_ptr<Chat::MessageData>> newMessages)
{
   // show notification of new message in tray icon
   for (auto i : newMessages) {

      auto userName = i.first;
      auto message = i.second;

      if (message) {
         const int maxMessageLength = 20;

         auto messageTitle = message->senderId();
         auto messageText = message->displayText();

         if (!userName.isEmpty()) {
            messageTitle = userName;
         }

         if (messageText.length() > maxMessageLength) {
            messageText = messageText.mid(0, maxMessageLength) + QLatin1String("...");
         }

         NotificationCenter::notify(bs::ui::NotifyType::UpdateUnreadMessage, {messageTitle, messageText, message->senderId()});
      }
   }
}

void ChatWidget::selectGlobalRoom()
{
   if (currentChat_.isEmpty()) {
      // find all indexes
      QModelIndexList indexes = ui_->treeViewUsers->model()->match(ui_->treeViewUsers->model()->index(0,0),
                                                                  Qt::DisplayRole,
                                                                  QLatin1String("*"),
                                                                  -1,
                                                                  Qt::MatchWildcard|Qt::MatchRecursive);
      // select Global room
      for (auto index : indexes) {
         if (index.data(ChatClientDataModel::Role::ItemTypeRole).value<ChatUIDefinitions::ChatTreeNodeType>() == ChatUIDefinitions::ChatTreeNodeType::RoomsElement) {
            if (index.data(ChatClientDataModel::Role::RoomIdRole).toString() == Chat::GlobalRoomKey) {
               ui_->treeViewUsers->setCurrentIndex(index);
               onRoomClicked(Chat::GlobalRoomKey);
               break;
            }
         }
      }
   }
}

void ChatWidget::onBSChatInputSelectionChanged()
{
   isChatMessagesSelected_ = false;
}

void ChatWidget::onChatMessagesSelectionChanged()
{
   isChatMessagesSelected_ = true;
}
