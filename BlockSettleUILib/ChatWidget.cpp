/*
#include "ChatWidget.h"
#include "ui_ChatWidget.h"

#include "ApplicationSettings.h"
#include "BSMessageBox.h"
#include "BSTerminalMainWindow.h"
#include "CelerClient.h"
#include "ChatClient.h"
#include "ChatClientDataModel.h"
#include "ChatProtocol/ChatUtils.h"
#include "ChatTreeModelWrapper.h"
#include "ImportKeyBox.h"
#include "NotificationCenter.h"
#include "OTCRequestViewModel.h"
#include "OtcClient.h"
#include "ZMQ_BIP15X_DataConnection.h"

#include <QApplication>
#include <QMouseEvent>
#include <QObject>
#include <QScrollBar>
#include <QClipboard>
#include <QMimeData>
#include <thread>
#include <spdlog/spdlog.h>
#include <ChatPartiesTreeModel.h>

#include "ChatProtocol/ClientPartyModel.h"

Q_DECLARE_METATYPE(std::vector<std::string>)

#define USE_NEW_TREE_MODEL
#ifdef USE_NEW_TREE_MODEL
#include <QTreeView>
#include <ChatClientUsersViewItemDelegate.h>
#endif

using namespace bs::network;
using namespace bs::network::otc;


// #old_logic : delete this all widget and use class RFQShieldPage(maybe need redo it but based class should be the same)
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
   OTCContactNetStatusShieldPage,
   OTCSupportRoomShieldPage
};

// #old_logic : do we need all this variables?
// #new_logic : redo this fro old one
ChatWidget::ChatWidget(QWidget *parent)
   : QWidget(parent)
   , ui_(new Ui::ChatWidget)
   , isContactRequest_(false)
   , needsToStartFirstRoom_(false)
   , chatLoggedInTimestampUtcInMillis_(0)
{
   ui_->setupUi(this);

#ifndef USE_NEW_TREE_MODEL
      ChatClientUserView* view;
      view = new ChatClientUserView(ui_->treeViewUsers->parentWidget());
      auto *pOld = ui_->treeViewUsers->parentWidget()->layout()->replaceWidget(ui_->treeViewUsers, view);
      ui_->treeViewUsers = view;
      pOld->widget()->setVisible(false);
#endif

#ifndef Q_OS_WIN
   ui_->timeLabel->setMinimumSize(ui_->timeLabel->property("minimumSizeLinux").toSize());
#endif

   ui_->textEditMessages->setColumnsWidth(ui_->timeLabel->minimumWidth(),
                                          ui_->iconLabel->minimumWidth(),
                                          ui_->userLabel->minimumWidth(),
                                          ui_->messageLabel->minimumWidth());

   //Init UI and other stuff
   ui_->frameContactActions->setVisible(false);
   ui_->stackedWidget->setCurrentIndex(1); //Basically stackedWidget should be removed

   otcRequestViewModel_ = new OTCRequestViewModel(this);
   ui_->treeViewOTCRequests->setModel(otcRequestViewModel_);

   qRegisterMetaType<std::vector<std::string>>();

   connect(ui_->pushButton_AcceptSend, &QPushButton::clicked, this, &ChatWidget::onContactRequestAcceptSendClicked);
   connect(ui_->pushButton_RejectCancel, &QPushButton::clicked, this, &ChatWidget::onContactRequestRejectCancelClicked);
}

ChatWidget::~ChatWidget() {
   // Should be done explicitly, since destructor for state could make changes inside chatWidget
   stateCurrent_.reset();
};

// #new_logic : redoing
void ChatWidget::init(const std::shared_ptr<ConnectionManager>& connectionManager
                 , const std::shared_ptr<ApplicationSettings> &appSettings
                 , const Chat::ChatClientServicePtr& chatClientServicePtr
                 , const std::shared_ptr<spdlog::logger>& logger
                 , const std::shared_ptr<bs::sync::WalletsManager> &walletsMgr
                 , const std::shared_ptr<ArmoryConnection> &armory
                 , const std::shared_ptr<SignContainer> &signContainer)
{
   logger_ = logger;
   client_ = std::make_shared<ChatClient>(connectionManager, appSettings, logger);

#ifndef USE_NEW_TREE_MODEL
  
   auto model = client_->getDataModel();
   model->setNewMessageMonitor(this);
   auto proxyModel = client_->getProxyModel();

   static_cast<ChatClientUserView*>(ui_->treeViewUsers)->setModel(proxyModel.get());
   static_cast<ChatClientUserView*>(ui_->treeViewUsers)->setSortingEnabled(true);
   static_cast<ChatClientUserView*>(ui_->treeViewUsers)->sortByColumn(0, Qt::AscendingOrder);
   connect(proxyModel.get(), &ChatTreeModelWrapper::treeUpdated,
           static_cast<ChatClientUserView*>(ui_->treeViewUsers), &QTreeView::expandAll);
   static_cast<ChatClientUserView*>(ui_->treeViewUsers)->expandAll();
   static_cast<ChatClientUserView*>(ui_->treeViewUsers)->addWatcher(ui_->textEditMessages);
   static_cast<ChatClientUserView*>(ui_->treeViewUsers)->addWatcher(this);
   static_cast<ChatClientUserView*>(ui_->treeViewUsers)->setHandler(this);
   static_cast<ChatClientUserView*>(ui_->treeViewUsers)->setActiveChatLabel(ui_->labelActiveChat);
#else
   // #new_logic
   chatClientServicePtr_ = chatClientServicePtr;

   chatPartiesTreeModel_ = std::make_shared<ChatPartiesTreeModel>(chatClientServicePtr_);
   connect(chatClientServicePtr_.get(), &Chat::ChatClientService::partyModelChanged,
      this, &ChatWidget::onPartyModelChanged);

   ChatPartiesSortProxyModelPtr charTreeSortModel = std::make_shared<ChatPartiesSortProxyModel>(chatPartiesTreeModel_);
   ui_->treeViewUsers->setModel(charTreeSortModel.get());
   ui_->treeViewUsers->sortByColumn(0, Qt::AscendingOrder);
   ui_->treeViewUsers->setSortingEnabled(true);
   ui_->treeViewUsers->setItemDelegate(new ChatClientUsersViewItemDelegate(charTreeSortModel, this));
   
   // connections
   // User actions
   connect(ui_->treeViewUsers, &QTreeView::clicked, this, &ChatWidget::onUserListClicked);
   connect(ui_->input_textEdit, &BSChatInput::sendMessage, this, &ChatWidget::onSendButtonClicked);
   connect(ui_->textEditMessages, &ChatMessagesTextEdit::messageRead, this, &ChatWidget::onMessageRead, Qt::QueuedConnection);

   // Back end changes
   connect(chatClientServicePtr_.get(), &Chat::ChatClientService::clientLoggedInToServer, this, &ChatWidget::onLogin);
   connect(chatClientServicePtr_.get(), &Chat::ChatClientService::clientLoggedOutFromServer, this, &ChatWidget::onLogout);
   connect(chatClientServicePtr_.get(), &Chat::ChatClientService::partyModelChanged, this, &ChatWidget::onPartyModelChanged);

   Chat::ClientPartyModelPtr chatModelPtr = chatClientServicePtr_->getClientPartyModelPtr();
   connect(chatModelPtr.get(), &Chat::ClientPartyModel::messageArrived, this, &ChatWidget::onSendArrived);
   connect(chatModelPtr.get(), &Chat::ClientPartyModel::clientPartyStatusChanged, this, &ChatWidget::onClientPartyStatusChanged);
   connect(chatModelPtr.get(), &Chat::ClientPartyModel::messageStateChanged, this, &ChatWidget::onMessageStateChanged);

   changeState<ChatLogOutState>(); //Initial state is LoggedOut
#endif
   ui_->textEditMessages->setHandler(this);
   ui_->textEditMessages->setMessageReadHandler(client_);
   ui_->textEditMessages->setClientPartyModel(chatModelPtr);
   ui_->input_textEdit->setAcceptRichText(false);

   //ui_->chatSearchLineEdit->setActionsHandler(client_);

   // #old_logic
   connect(client_.get(), &ChatClient::LoginFailed, this, &ChatWidget::onLoginFailed);
   connect(client_.get(), &ChatClient::ConfirmContactsNewData, this, &ChatWidget::onContactListConfirmationRequested);
   connect(client_.get(), &ChatClient::LoggedOut, this, &ChatWidget::onLoggedOut);
   
   connect(client_.get(), &ChatClient::ContactRequestAccepted, this, &ChatWidget::onContactRequestAccepted);
   connect(client_.get(), &ChatClient::RoomsInserted, this, &ChatWidget::selectGlobalRoom);
   connect(client_.get(), &ChatClient::NewContactRequest, this, [=] (const std::string &userId) {
            NotificationCenter::notify(bs::ui::NotifyType::FriendRequest, {QString::fromStdString(userId)});
            onContactChanged();
   });
   connect(client_.get(), &ChatClient::ConfirmUploadNewPublicKey, this, &ChatWidget::onConfirmUploadNewPublicKey);
   connect(client_.get(), &ChatClient::ContactChanged, this, &ChatWidget::onContactChanged);
   connect(client_.get(), &ChatClient::DMMessageReceived, this, &ChatWidget::onDMMessageReceived);
   connect(client_.get(), &ChatClient::ContactRequestApproved, this, &ChatWidget::onContactRequestApproved);

   
   connect(ui_->input_textEdit, &BSChatInput::selectionChanged, this, &ChatWidget::onBSChatInputSelectionChanged);

   connect(ui_->textEditMessages, &QTextEdit::selectionChanged, this, &ChatWidget::onChatMessagesSelectionChanged);
   connect(ui_->textEditMessages, &ChatMessagesTextEdit::addContactRequired, this, &ChatWidget::onSendFriendRequest);

   //connect(ui_->chatSearchLineEdit, &ChatSearchLineEdit::returnPressed, this, &ChatWidget::onSearchUserReturnPressed);

   connect(ui_->treeViewOTCRequests->selectionModel(), &QItemSelectionModel::selectionChanged
      , this, &ChatWidget::OnOTCSelectionChanged);

   
   initSearchWidget();

   installEventFilter(this);

   otcClient_ = new OtcClient(logger_, walletsMgr, armory, signContainer, this);
   connect(client_.get(), &ChatClient::contactConnected, otcClient_, &OtcClient::peerConnected);
   connect(client_.get(), &ChatClient::contactDisconnected, otcClient_, &OtcClient::peerDisconnected);
   connect(client_.get(), &ChatClient::otcMessageReceived, otcClient_, &OtcClient::processMessage);
   connect(otcClient_, &OtcClient::sendPbMessage, this, &ChatWidget::sendOtcPbMessage);

   connect(otcClient_, &OtcClient::sendMessage, client_.get(), &ChatClient::sendOtcMessage);

   connect(otcClient_, &OtcClient::peerUpdated, this, &ChatWidget::onOtcUpdated);

   connect(ui_->widgetNegotiateRequest, &OTCNegotiationRequestWidget::requestCreated, this, &ChatWidget::onOtcRequestSubmit);
   connect(ui_->widgetPullOwnOTCRequest, &PullOwnOTCRequestWidget::requestPulled, this, &ChatWidget::onOtcRequestPull);
   connect(ui_->widgetNegotiateResponse, &OTCNegotiationResponseWidget::responseAccepted, this, &ChatWidget::onOtcResponseAccept);
   connect(ui_->widgetNegotiateResponse, &OTCNegotiationResponseWidget::responseUpdated, this, &ChatWidget::onOtcResponseUpdate);
   connect(ui_->widgetNegotiateResponse, &OTCNegotiationResponseWidget::responseRejected, this, &ChatWidget::onOtcResponseReject);
}


void ChatWidget::onAddChatRooms(const std::vector<std::shared_ptr<Chat::Data> >& roomList)
{
   if (roomList.size() > 0 && needsToStartFirstRoom_) {
      // ui_->treeViewUsers->selectFirstRoom();
      const auto &firstRoom = roomList.at(0);
      needsToStartFirstRoom_ = false;
   }
}

void ChatWidget::onUsersDeleted(const std::vector<std::string> &users)
{
   //stateCurrent_->onUsersDeleted(users);
}

void ChatWidget::onContactRequestApproved(const std::string &userId)
{
#ifndef USE_NEW_TREE_MODEL
   static_cast<ChatClientUserView*>(ui_->treeViewUsers)->setCurrentUserChat(userId);
   static_cast<ChatClientUserView*>(ui_->treeViewUsers)->updateCurrentChat();
#endif
}

//void ChatWidget::changeState(ChatWidget::State state)
//{
//
//   //Do not add any functionality here, except  states swapping
//
//   if (!stateCurrent_) { //In case if we use change state in first time
//      stateCurrent_ = std::make_shared<ChatWidgetStateLoggedOut>(this);
//      stateCurrent_->onStateEnter();
//   } else if (stateCurrent_->type() != state) {
//      stateCurrent_->onStateExit();
//      switch (state) {
//      case State::LoggedIn:
//         {
//            stateCurrent_ = std::make_shared<ChatWidgetStateLoggedIn>(this);
//         }
//         break;
//      case State::LoggedOut:
//         {
//            stateCurrent_ = std::make_shared<ChatWidgetStateLoggedOut>(this);
//         }
//         break;
//      }
//      stateCurrent_->onStateEnter();
//   }
//}

void ChatWidget::initSearchWidget()
{
   ui_->searchWidget->init(client_);

   connect(ui_->searchWidget, &SearchWidget::addFriendRequied,
           this, &ChatWidget::onSendFriendRequest);
   //connect(ui_->searchWidget, &SearchWidget::showUserRoom,
   //        this, &ChatWidget::onChangeChatRoom);
}

bool ChatWidget::isLoggedIn() const
{
   if (!stateCurrent_) {
      return false;
   }
   //return stateCurrent_->type() == State::LoggedIn;
   return false;
}

void ChatWidget::onSendButtonClicked()
{
   return stateCurrent_->sendMessage();
}

void ChatWidget::onMessagesUpdated()
{
   //return stateCurrent_->onMessagesUpdated();
}

std::string ChatWidget::login(const std::string& email, const std::string& jwt
   , const ZmqBipNewKeyCb &cb)
{
   try {
      //const auto userId = stateCurrent_->login(email, jwt, cb);
      needsToStartFirstRoom_ = true;
      //return userId;
      return "";
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
   //stateCurrent_->onLoginFailed();
   emit LoginFailed();
}

void ChatWidget::logout()
{
   //return stateCurrent_->logout();
}

bool ChatWidget::hasUnreadMessages()
{
   return true;
}

void ChatWidget::setCelerClient(std::shared_ptr<BaseCelerClient> celerClient)
{
   celerClient_ = celerClient;
}

void ChatWidget::updateChat(const bool &isChatTab)
{
   isChatTab_ = isChatTab;

   // #old_logic
   //ui_->textEditMessages->setIsChatTab(isChatTab_);

   if (isChatTab_) {
#ifndef USE_NEW_TREE_MODEL
   static_cast<ChatClientUserView*>(ui_->treeViewUsers)->updateCurrentChat();
#else

#endif
   }
}

void ChatWidget::onLoggedOut()
{
   //stateCurrent_->onLoggedOut();
   //emit LogOut();
}

void ChatWidget::onNewChatMessageTrayNotificationClicked(const QString &userId)
{
#ifndef USE_NEW_TREE_MODEL
   static_cast<ChatClientUserView*>(ui_->treeViewUsers)->setCurrentUserChat(userId.toStdString());
#endif
   ui_->input_textEdit->setFocus(Qt::FocusReason::MouseFocusReason);
}

void ChatWidget::processOtcPbMessage(const std::string &data)
{
   otcClient_->processPbMessage(data);
}

void ChatWidget::onConnectedToServer()
{
   const auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
   chatLoggedInTimestampUtcInMillis_ =  timestamp.count();
   otcClient_->setCurrentUserId(client_->currentUserId());
}

void ChatWidget::onContactRequestAccepted(const std::string &userId)
{
#ifndef USE_NEW_TREE_MODEL
   static_cast<ChatClientUserView*>(ui_->treeViewUsers)->setCurrentUserChat(userId);
#endif
}

void ChatWidget::onConfirmUploadNewPublicKey(const std::string &oldKey, const std::string &newKey)
{
   const auto deferredDialog = [this, oldKey, newKey]{
      ImportKeyBox box(BSMessageBox::question
                       , tr("Update OTC ID Key?")
                       , this);

      box.setAddrPort("");
      box.setDescription(QStringLiteral("Unless your OTC ID Key is lost or compromised, "
         "BlockSettle strongly discourages from re-submitting a new OTC ID Key. "
         "When updating your OTC ID Key, all your contacts will be asked to replace the OTC ID Key they use in relation to communication with yourself. "
         "You will need to rebuild and re-establish your reputation. Are you sure you wish to continue?"));
      box.setNewKeyFromBinary(newKey);
      box.setOldKeyFromBinary(oldKey);
      box.setCancelVisible(true);

      bool confirmed = box.exec() == QDialog::Accepted;
      client_->uploadNewPublicKeyToServer(confirmed);
   };

   for (QWidget *widget : qApp->topLevelWidgets()) {
      BSTerminalMainWindow *mainWindow = qobject_cast<BSTerminalMainWindow *>(widget);
      if (mainWindow) {
         mainWindow->addDeferredDialog(deferredDialog);
         break;
      }
   }
}

void ChatWidget::onConfirmContactNewKeyData(
      const std::vector<std::shared_ptr<Chat::Data> > &remoteConfirmed
      , const std::vector<std::shared_ptr<Chat::Data> > &remoteKeysUpdate
      , const std::vector<std::shared_ptr<Chat::Data> > &remoteAbsolutelyNew
      , bool bForceUpdateAllUsers)
{
   Q_UNUSED(remoteConfirmed)

   const auto localContacts = client_->getDataModel()->getAllContacts();
   std::map<std::string, std::shared_ptr<Chat::Data> > dict;
   for (const auto &contact : localContacts) {
      dict[contact->mutable_contact_record()->contact_id()] = contact;
   }

   std::vector<std::shared_ptr<Chat::Data> > updateList;
   for (const auto &contact : remoteKeysUpdate) {
      if (!contact || !contact->has_contact_record()) {
         logger_->error("[ChatWidget::{}] invalid contact", __func__);
         continue;
      }
      auto contactRecord = contact->mutable_contact_record();
      auto oldContact = client_->getContact(contactRecord->contact_id());
      if (oldContact.contact_id().empty()) {
         logger_->error("[ChatWidget::{}] invalid contact", __func__);
         continue;
      }
      auto name = QString::fromStdString(contactRecord->display_name());
      if (name.isEmpty()) {
         name = QString::fromStdString(contactRecord->contact_id());
      }

      if (bForceUpdateAllUsers) {
         updateList.push_back(contact);
         continue;
      }

      ImportKeyBox box(BSMessageBox::question
                       , tr("Import Contact '%1' Public Key?").arg(name)
                       , this);
      box.setAddrPort(std::string());
      box.setNewKeyFromBinary(contactRecord->public_key());
      box.setOldKeyFromBinary(oldContact.public_key());
      box.setCancelVisible(true);

      if (box.exec() == QDialog::Accepted) {
         updateList.push_back(contact);
      } else {
         auto userId = contact->mutable_contact_record()->contact_id();
         auto it = dict.find(userId);
         if (it != dict.end()) {
            dict.erase(it);
         }
         // New public key rejected, let's remove contact from friends
         client_->OnContactNewPublicKeyRejected(userId);
      }
   }

   std::vector<std::shared_ptr<Chat::Data> > leaveList;
   for (const auto &item : dict) {
      leaveList.push_back(item.second);
   }

   std::vector<std::shared_ptr<Chat::Data> > newList;
   for (const auto &contact : remoteAbsolutelyNew) {
      if (!contact || !contact->has_contact_record()) {
         logger_->error("[ChatWidget::{}] invalid contact", __func__);
         continue;
      }
      auto contactRecord = contact->mutable_contact_record();
      auto name = QString::fromStdString(contactRecord->contact_id());

      if (bForceUpdateAllUsers) {
         newList.push_back(contact);
         continue;
      }

      ImportKeyBox box(BSMessageBox::question
                       , tr("Do you wish to keep or remove '%1' as a Contact? ").arg(name)
                       , this);
      box.setWindowTitle(tr("Import Contact ID Key"));
      box.setAddrPort(std::string());
      box.setNewKeyFromBinary(contactRecord->public_key());
      box.setOldKey(std::string());
      box.setCancelVisible(true);
      box.setConfirmButtonText(tr("Keep"));
      box.setCancelButtonText(tr("Remove"));

      if (box.exec() == QDialog::Accepted) {
         newList.push_back(contact);
      }
   }

   client_->OnContactListConfirmed(leaveList, updateList, newList);
}

bool ChatWidget::eventFilter(QObject *sender, QEvent *event)
{
   if (event->type() == QEvent::WindowActivate) {
      // hide tab icon on window activate event
      NotificationCenter::notify(bs::ui::NotifyType::UpdateUnreadMessage, {});

      if (isChatTab_) {
#ifndef USE_NEW_TREE_MODEL
   static_cast<ChatClientUserView*>(ui_->treeViewUsers)->updateCurrentChat();
#endif
      }
   }

   // copy selected messages by keyboard shortcut
   if (event->type() == QEvent::KeyPress) {
      QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);

      // handle ctrl+c (cmd+c on macOS)
      if(keyEvent->key() == Qt::Key_C && keyEvent->modifiers().testFlag(Qt::ControlModifier)) {
         if (ui_->textEditMessages->textCursor().hasSelection()) {
            QApplication::clipboard()->setText(ui_->textEditMessages->getFormattedTextFromSelection());
            return true;
         }
      }
   }

   return QWidget::eventFilter(sender, event);
}

void ChatWidget::onSendFriendRequest(const QString &userId)
{
   //client_->sendFriendRequest(userId.toStdString());
   onActionCreatePendingOutgoing (userId.toStdString());
#ifndef USE_NEW_TREE_MODEL
   static_cast<ChatClientUserView*>(ui_->treeViewUsers)->setCurrentUserChat(userId.toStdString());
   static_cast<ChatClientUserView*>(ui_->treeViewUsers)->updateCurrentChat();
#endif

   ui_->searchWidget->setListVisible(false);
}

bool ChatWidget::isRoom()
{
   return isRoom_;
}

bool ChatWidget::isContactRequest()
{
   return isContactRequest_;
}

void ChatWidget::setIsContactRequest(bool isCr)
{
   isContactRequest_ = isCr;
}

void ChatWidget::setIsRoom(bool isRoom)
{
   isRoom_ = isRoom;
}

void ChatWidget::onMessageChanged(std::shared_ptr<Chat::Data> message)
{
}

void ChatWidget::onElementUpdated(CategoryElement *element)
{
   if (element) {
      switch (element->getType()) {
         case ChatUIDefinitions::ChatTreeNodeType::RoomsElement: {
            //TODO: Change cast
            auto room = element->getDataObject();
            if (room && room->has_room() && currentChat_ == room->room().id()) {
               OTCSwitchToRoom(room);
            }
         }
         break;
         case ChatUIDefinitions::ChatTreeNodeType::ContactsRequestElement: {
            auto contact = element->getDataObject();
            if (contact && contact->has_contact_record()) {
               ChatContactElement *cElement = dynamic_cast<ChatContactElement*>(element);
               //Hide buttons if this current chat, but thme shouldn't be visible
               bool isButtonsVisible = currentChat_ == contact->contact_record().contact_id() &&
                  (cElement->getContactData()->status() == Chat::ContactStatus::CONTACT_STATUS_OUTGOING_PENDING
                     || cElement->getContactData()->status() == Chat::ContactStatus::CONTACT_STATUS_INCOMING);
               ui_->frameContactActions->setVisible(isButtonsVisible);
            }
         }
         break;
         default:
            break;
      }
   }
}

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
   return celerClient_ && celerClient_->IsConnected() &&
      (celerClient_->celerUserType() == BaseCelerClient::CelerUserType::Dealing
         || celerClient_->celerUserType() == BaseCelerClient::CelerUserType::Trading);
}

void ChatWidget::clearCursorSelection(QTextEdit *element)
{
   auto cursor = element->textCursor();
   cursor.clearSelection();
   element->setTextCursor(cursor);
}

void ChatWidget::updateOtc(const std::string &contactId)
{
   auto peer = otcClient_->peer(contactId);
   if (!peer) {
      ui_->stackedWidgetOTC->setCurrentIndex(static_cast<int>(OTCPages::OTCContactNetStatusShieldPage));
      return;
   }

   switch (peer->state) {
      case otc::State::Idle:
         ui_->stackedWidgetOTC->setCurrentIndex(static_cast<int>(OTCPages::OTCNegotiateRequestPage));
         break;
      case otc::State::OfferSent:
         ui_->widgetPullOwnOTCRequest->setOffer(peer->offer);
         ui_->stackedWidgetOTC->setCurrentIndex(static_cast<int>(OTCPages::OTCPullOwnOTCRequestPage));
         break;
      case otc::State::OfferRecv:
         ui_->widgetNegotiateResponse->setOffer(peer->offer);
         ui_->stackedWidgetOTC->setCurrentIndex(static_cast<int>(OTCPages::OTCNegotiateResponsePage));
         break;
      case otc::State::SentPayinInfo:
      case otc::State::WaitPayinInfo:
         ui_->stackedWidgetOTC->setCurrentIndex(static_cast<int>(OTCPages::OTCContactNetStatusShieldPage));
         break;
      case otc::State::Blacklisted:
         ui_->stackedWidgetOTC->setCurrentIndex(static_cast<int>(OTCPages::OTCContactNetStatusShieldPage));
         break;
   }
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

void ChatWidget::OTCSwitchToSupportRoom()
{
   ui_->stackedWidgetOTC->setCurrentIndex(static_cast<int>(OTCPages::OTCSupportRoomShieldPage));
}

void ChatWidget::OTCSwitchToRoom(std::shared_ptr<Chat::Data>& room)
{
   assert(room->has_room());

   // #old_logic
   //if (room->room().is_trading_available()) {
   //   ui_->stackedWidgetMessages->setCurrentIndex(1);
   //   OTCSwitchToCommonRoom();
   //} else {
   //   ui_->stackedWidgetMessages->setCurrentIndex(0);
   //   ui_->input_textEdit->setFocus();
   //   if (IsGlobalChatRoom(room->room().id())) {
   //      OTCSwitchToGlobalRoom();
   //   } else if (IsSupportChatRoom(room->room().id())) {
   //      OTCSwitchToSupportRoom();
   //   }
   //}
}

void ChatWidget::OTCSwitchToContact(std::shared_ptr<Chat::Data>& contact)
{
   assert(contact->has_contact_record());

   ui_->input_textEdit->setFocus();

   if (!TradingAvailableForUser()) {
      ui_->stackedWidgetOTC->setCurrentIndex(static_cast<int>(OTCPages::OTCParticipantShieldPage));
      return;
   }

   if (contact->contact_record().status() != Chat::CONTACT_STATUS_ACCEPTED) {
      ui_->stackedWidgetOTC->setCurrentIndex(static_cast<int>(OTCPages::OTCContactShieldPage));
      return;
   }

   updateOtc(contact->contact_record().contact_id());
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

void ChatWidget::onOtcRequestSubmit()
{
   bool result = otcClient_->sendOffer(ui_->widgetNegotiateRequest->offer(), currentChat_);
   if (!result) {
      SPDLOG_LOGGER_ERROR(logger_, "send offer failed");
      return;
   }
}

void ChatWidget::onOtcRequestPull()
{
   bool result = otcClient_->pullOrRejectOffer(currentChat_);
   if (!result) {
      SPDLOG_LOGGER_ERROR(logger_, "pull offer failed");
      return;
   }
}

void ChatWidget::onOtcResponseAccept()
{
   bool result = otcClient_->acceptOffer(ui_->widgetNegotiateResponse->offer(), currentChat_);
   if (!result) {
      SPDLOG_LOGGER_ERROR(logger_, "accept offer failed");
      return;
   }
}

void ChatWidget::onOtcResponseUpdate()
{
   bool result = otcClient_->updateOffer(ui_->widgetNegotiateResponse->offer(), currentChat_);
   if (!result) {
      SPDLOG_LOGGER_ERROR(logger_, "update offer failed");
      return;
   }
}

void ChatWidget::onOtcResponseReject()
{
   bool result = otcClient_->pullOrRejectOffer(currentChat_);
   if (!result) {
      SPDLOG_LOGGER_ERROR(logger_, "reject offer failed");
      return;
   }
}

void ChatWidget::onOtcUpdated(const std::string &contactId)
{
   if (contactId == currentChat_) {
      updateOtc(currentChat_);
   }
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
   // #old_logic
   return false;
}

void ChatWidget::onNewMessagesPresent(std::map<std::string, std::shared_ptr<Chat::Data>> newMessages)
{
   // show notification of new message in tray icon
   for (auto i : newMessages) {

      auto userName = i.first;
      auto message = i.second;

      if (message) {
         auto messageTitle = userName.empty() ? message->message().sender_id() : userName;
         auto messageText = (message->message().encryption() == Chat::Data_Message_Encryption_UNENCRYPTED)
               ? message->message().message() : "";

         // #old_logic : do we need it in global?
         int maxMessageLength = 20;
         if (messageText.length() > maxMessageLength) {
            messageText = messageText.substr(0, maxMessageLength) + "...";
         }

         bs::ui::NotifyMessage notifyMsg;
         notifyMsg.append(QString::fromStdString(messageTitle));
         notifyMsg.append(QString::fromStdString(messageText));
         notifyMsg.append(QString::fromStdString(message->message().sender_id()));

         NotificationCenter::notify(bs::ui::NotifyType::UpdateUnreadMessage, notifyMsg);
      }
   }
}

void ChatWidget::selectGlobalRoom()
{
   if (currentChat_.empty()) {
      // find all indexes
#ifndef USE_NEW_TREE_MODEL
      QModelIndexList indexes = static_cast<ChatClientUserView*>(ui_->treeViewUsers)->model()->match(
               static_cast<ChatClientUserView*>(ui_->treeViewUsers)->model()->index(0,0),
                                                                  Qt::DisplayRole,
                                                                  QLatin1String("*"),
                                                                  -1,
                                                                  Qt::MatchWildcard|Qt::MatchRecursive);
      // select Global room
      for (auto index : indexes) {
         if (index.data(ChatClientDataModel::Role::ItemTypeRole).value<ChatUIDefinitions::ChatTreeNodeType>() == ChatUIDefinitions::ChatTreeNodeType::RoomsElement) {
            if (index.data(ChatClientDataModel::Role::RoomIdRole).toString().toStdString() == ChatUtils::GlobalRoomKey) {
               static_cast<ChatClientUserView*>(ui_->treeViewUsers)->setCurrentIndex(index);
               break;
            }
         }
      }
#endif
   }
}

void ChatWidget::onBSChatInputSelectionChanged()
{
   // Once we got new selection we should clear previous one.
   clearCursorSelection(ui_->textEditMessages);
}

void ChatWidget::onChatMessagesSelectionChanged()
{
   // Once we got new selection we should clear previous one.
   clearCursorSelection(ui_->input_textEdit);
}

void ChatWidget::onActionCreatePendingOutgoing(const std::string &userId)
{
   return client_->createPendingFriendRequest(userId);
   //return client_->sendFriendRequest(userId, std::string("I would like to add you to friends!"));
}

void ChatWidget::onActionRemoveFromContacts(std::shared_ptr<Chat::Data> crecord)
{
   return client_->removeFriendOrRequest(crecord->contact_record().contact_id());
}

void ChatWidget::onActionAcceptContactRequest(std::shared_ptr<Chat::Data> crecord)
{
   return client_->acceptFriendRequest(crecord->contact_record().contact_id());
}

void ChatWidget::onActionRejectContactRequest(std::shared_ptr<Chat::Data> crecord)
{
    return client_->rejectFriendRequest(crecord->contact_record().contact_id());
}

void ChatWidget::onActionEditContactRequest(std::shared_ptr<Chat::Data> crecord)
{
   return client_->onEditContactRequest(crecord);
}

bool ChatWidget::onActionIsFriend(const std::string &userId)
{
   return client_->isFriend(userId);
}

void ChatWidget::onContactRequestAcceptSendClicked()
{
   std::string messageText = ui_->input_textEdit->toPlainText().toStdString();
   ui_->input_textEdit->clear();
   ui_->input_textEdit->setReadOnly(false);
   client_->onContactRequestPositiveAction(currentChat_, messageText);
}

void ChatWidget::onContactRequestRejectCancelClicked()
{
   ui_->input_textEdit->clear();
   client_->onContactRequestNegativeAction(currentChat_);
}

void ChatWidget::onContactListConfirmationRequested(const std::vector<std::shared_ptr<Chat::Data> > &remoteConfirmed,
                                                    const std::vector<std::shared_ptr<Chat::Data> > &remoteKeysUpdate,
                                                    const std::vector<std::shared_ptr<Chat::Data> > &remoteAbsolutelyNew)
{
   QString detailsPattern = QLatin1String("Confirmed contacts: %1\n"
                                          "Require key update: %2\n"
                                          "New contacts: %3");

   QString  detailsString = detailsPattern.arg(remoteConfirmed.size()).arg(remoteKeysUpdate.size()).arg(remoteAbsolutelyNew.size());

   BSMessageBox bsMessageBox(BSMessageBox::question, tr("Contacts Information Update"),
      tr("Do you wish to import your full Contact list?"),
      tr("Press OK to Import all Contact ID keys. Selecting Cancel will allow you to determine each contact individually."),
      detailsString);
   int ret = bsMessageBox.exec();

   onConfirmContactNewKeyData(remoteConfirmed, remoteKeysUpdate,
                              remoteAbsolutelyNew, QDialog::Accepted == ret);
}

void ChatWidget::onContactChanged()
{
   updateChat(true);
}

void ChatWidget::onCurrentElementAboutToBeRemoved()
{
   // To make selectGlobalRoom(); workable
   currentChat_.clear();

   selectGlobalRoom();
}

void ChatWidget::onDMMessageReceived(const std::shared_ptr<Chat::Data>& message)
{
   if (isVisible() && isActiveWindow()) {
      return;
   }
   std::map<std::string, std::shared_ptr<Chat::Data>> newMessages;
   const auto model = client_->getDataModel();
   std::string contactName = model->getContactDisplayName(message->message().sender_id());
   newMessages.emplace(contactName, message);
   model->getNewMessageMonitor()->onNewMessagesPresent(newMessages);
}



// #new_logic
void ChatWidget::onUserListClicked(const QModelIndex& index)
{
   // #new_logic : save draft

   ChatPartiesSortProxyModel *chartProxyModel = static_cast<ChatPartiesSortProxyModel *>(ui_->treeViewUsers->model());
   PartyTreeItem* partyTreeItem = chartProxyModel->getInternalData(index);

   if (partyTreeItem->modelType() == UI::ElementType::Container) {
      currentChat_.clear();
      changeState<IdleState>();
      return;
   }

   const auto clientPartyPtr = partyTreeItem->data().value<Chat::ClientPartyPtr>();

   auto transitionChange = [this, newChat = clientPartyPtr->id()]() {
      currentChat_ = newChat;
   };
   switch (clientPartyPtr->partyState())
   {
   case Chat::PartyState::UNINITIALIZED:
      changeState<PrivatePartyUninitState>(transitionChange);
      break;
   case Chat::PartyState::REQUESTED:
      if (clientPartyPtr->id() == chatClientServicePtr_->getClientPartyModelPtr()->ownUserName()) {
         changeState<PrivatePartyRequestedOutgoingState>(transitionChange);
      }
      else {
         changeState<PrivatePartyRequestedIncomingState>(transitionChange);
      }
      break;
   case Chat::PartyState::REJECTED:
      changeState<PrivatePartyRejectedState>(transitionChange);
      break;
   case Chat::PartyState::INITIALIZED:
      changeState<PrivatePartyInitState>(transitionChange);
      break;
   default:
      break;
   }
}

void ChatWidget::onSendMessage()
{
   stateCurrent_->sendMessage();
}

void ChatWidget::onMessageRead(const std::string& partyId, const std::string& messageId)
{
   stateCurrent_->messageRead(partyId, messageId);
}

void ChatWidget::onLogin()
{
   currentChat_ = "Global";
   ui_->textEditMessages->switchToChat(currentChat_);
   changeState<IdleState>();
}

void ChatWidget::onLogout()
{
   changeState<ChatLogOutState>();
}

void ChatWidget::onSendArrived(const Chat::MessagePtrList& messagePtr)
{
   stateCurrent_->messageArrived(messagePtr);
}

void ChatWidget::onClientPartyStatusChanged(const Chat::ClientPartyPtr& clientPartyPtr)
{
   stateCurrent_->changePartyStatus(clientPartyPtr);
}

void ChatWidget::onPartyModelChanged()
{
   stateCurrent_->resetPartyModel();
}

void ChatWidget::onMessageStateChanged(const std::string& partyId, const std::string& message_id, const int party_message_state)
{
   stateCurrent_->changeMessageState(partyId, message_id, party_message_state);
}
*/

#include <QTreeView>
#include <QFrame>
#include <QAbstractScrollArea>
#include <QUrl>
#include <QTextDocument>
#include <QTextOption>
#include <QPen>
#include <QTextFormat>
#include <QTextCursor>
#include <QTextEdit>

#include "ChatClientUsersViewItemDelegate.h"
//#include "OTCRequestViewModel.h"
#include "BSChatInput.h"

#include "ChatWidget.h"
#include "ui_ChatWidget.h"

// #old_logic : delete this all widget and use class RFQShieldPage(maybe need redo it but based class should be the same)
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
   OTCContactNetStatusShieldPage,
   OTCSupportRoomShieldPage
};

// #new_logic : make this class QObject in different file
class AbstractChatWidgetState {
public:
   explicit AbstractChatWidgetState(ChatWidget* chat) : chat_(chat) { }
   virtual ~AbstractChatWidgetState() = default;

   void enterState() {
      applyUserFrameChange();
      applyChatFrameChange();
      applyRoomsFrameChange();
   }

   // slots
public:
   void sendMessage() {
      if (!canSendMessage()) {
         Q_ASSERT(false); // "It should not be possible to send message in this state."
         return;
      }

      std::string messageText = chat_->ui_->input_textEdit->toPlainText().toStdString();
      chat_->chatClientServicePtr_->SendPartyMessage(chat_->currentChat_, messageText);
      chat_->ui_->input_textEdit->clear();
   }
   void messageArrived(const Chat::MessagePtrList& messagePtr) {
      if (!canReceiveMessage()) {
         Q_ASSERT(false); // "It should not be possible to send message in this state."
         return;
      }

      chat_->ui_->textEditMessages->onSingleMessageUpdate(messagePtr);
   }
   void changePartyStatus(const Chat::ClientPartyPtr& clientPartyPtr) {
      if (!canChangePartyStatus()) {
         return;
      }

      chat_->chatPartiesTreeModel_->partyStatusChanged(clientPartyPtr);
   }
   void resetPartyModel() {
      if (!canResetPartyModel()) {
         return;
      }

      chat_->chatPartiesTreeModel_->partyModelChanged();
   }
   void messageRead(const std::string& partyId, const std::string& messageId) {
      if (!canResetReadMessage()) {
         return;
      }

      chat_->chatClientServicePtr_->SetMessageSeen(partyId, messageId);
   }
   void changeMessageState(const std::string& partyId, const std::string& message_id, const int party_message_state) {
      if (!canChangeMessageState()) {
         return;
      }

      chat_->ui_->textEditMessages->onMessageStatusChanged(partyId, message_id, party_message_state);
   }

protected:
   virtual void applyUserFrameChange() = 0;
   virtual void applyChatFrameChange() = 0;
   virtual void applyRoomsFrameChange() = 0;

   virtual bool canSendMessage() const { return false; }
   virtual bool canReceiveMessage() const { return true; }
   virtual bool canChangePartyStatus() const { return true; }
   virtual bool canResetPartyModel() const { return true; }
   virtual bool canResetReadMessage() const { return true; }
   virtual bool canChangeMessageState() const { return true; }

   void saveDraftMessage()
   {
      const auto draft = chat_->ui_->input_textEdit->toPlainText();

      if (draft.isEmpty()) 
      {
         chat_->draftMessages_.remove(chat_->currentChat_);
      }
      else
      {
         chat_->draftMessages_.insert(chat_->currentChat_, draft);
      }
   }

   void restoreDraftMessage() {
      const auto iDraft = chat_->draftMessages_.find(chat_->currentChat_);
      if (iDraft != chat_->draftMessages_.cend()) {
         chat_->ui_->input_textEdit->setText(iDraft.value());
         auto cursor = chat_->ui_->input_textEdit->textCursor();
         cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::MoveAnchor);
         chat_->ui_->input_textEdit->setTextCursor(cursor);
      }
   }

   ChatWidget* chat_;
};

class ChatLogOutState : public AbstractChatWidgetState {
public:
   explicit ChatLogOutState(ChatWidget* chat) : AbstractChatWidgetState(chat) { enterState(); }
   virtual ~ChatLogOutState() = default;
protected:
   virtual void applyUserFrameChange() override {
      auto* searchWidget = chat_->ui_->searchWidget;
      searchWidget->clearLineEdit();
      searchWidget->setLineEditEnabled(false);

      chat_->chatPartiesTreeModel_->cleanModel();

      chat_->ui_->labelUserName->setText(QObject::tr("offline"));
   }
   virtual void applyChatFrameChange() override {
      chat_->ui_->textEditMessages->logout();

      chat_->ui_->frameContactActions->setVisible(false);

      chat_->ui_->input_textEdit->setText(QLatin1Literal(""));
      chat_->ui_->input_textEdit->setVisible(false);
      chat_->ui_->input_textEdit->setEnabled(false);

      chat_->draftMessages_.clear();
   }
   virtual void applyRoomsFrameChange() override {
      chat_->ui_->stackedWidgetOTC->setCurrentIndex(static_cast<int>(OTCPages::OTCLoginRequiredShieldPage));
   }

   virtual bool canReceiveMessage() const override { return false; }
   virtual bool canChangePartyStatus() const override { return false; }
   virtual bool canResetReadMessage() const override { return false; }
   virtual bool canChangeMessageState() const override { return false; }
};

class IdleState : public AbstractChatWidgetState {
public:
   explicit IdleState(ChatWidget* chat) : AbstractChatWidgetState(chat) { enterState(); }
   virtual ~IdleState() override = default;
protected:
   virtual void applyUserFrameChange() override {
      chat_->ui_->searchWidget->setLineEditEnabled(true);

      const auto chatModelPtr = chat_->chatClientServicePtr_->getClientPartyModelPtr();
      chat_->ui_->labelUserName->setText(QString::fromStdString(chatModelPtr->ownUserName()));
   }
   virtual void applyChatFrameChange() override {
      const auto chatModelPtr = chat_->chatClientServicePtr_->getClientPartyModelPtr();
      chat_->ui_->textEditMessages->setOwnUserId(chatModelPtr->ownUserName());
      chat_->ui_->textEditMessages->resetChatView();

      chat_->ui_->frameContactActions->setVisible(false);

      chat_->ui_->input_textEdit->setText({});
      chat_->ui_->input_textEdit->setVisible(true);
      chat_->ui_->input_textEdit->setEnabled(false);
   }
   virtual void applyRoomsFrameChange() override {
      chat_->ui_->stackedWidgetOTC->setCurrentIndex(static_cast<int>(OTCPages::OTCGeneralRoomShieldPage));
   }
};

class PrivatePartyInitState : public AbstractChatWidgetState {
public:
   explicit PrivatePartyInitState(ChatWidget* chat) : AbstractChatWidgetState(chat) { enterState(); }
   virtual ~PrivatePartyInitState() override {
      saveDraftMessage();
   };
protected:
   virtual void applyUserFrameChange() override {}
   virtual void applyChatFrameChange() override {
      chat_->ui_->textEditMessages->switchToChat(chat_->currentChat_);

      chat_->ui_->frameContactActions->setVisible(false);

      // #new_logic : draft ??
      chat_->ui_->input_textEdit->setText({});
      chat_->ui_->input_textEdit->setVisible(true);
      chat_->ui_->input_textEdit->setEnabled(true);

      restoreDraftMessage();
   }
   virtual void applyRoomsFrameChange() override {
      // #new_logic : OTC shield
   }
   virtual bool canSendMessage() const override { return true; }
};

class PrivatePartyUninitState : public AbstractChatWidgetState {
public:
   explicit PrivatePartyUninitState(ChatWidget* chat) : AbstractChatWidgetState(chat) { enterState(); }
   virtual ~PrivatePartyUninitState() override = default;
protected:
   virtual void applyUserFrameChange() override {}
   virtual void applyChatFrameChange() override {
      chat_->ui_->textEditMessages->switchToChat(chat_->currentChat_);

      chat_->ui_->pushButton_AcceptSend->setText(QObject::tr("SEND"));
      chat_->ui_->pushButton_RejectCancel->setText(QObject::tr("CANCEL"));
      chat_->ui_->frameContactActions->setVisible(true);

      chat_->ui_->input_textEdit->setText({});
      chat_->ui_->input_textEdit->setVisible(true);
      chat_->ui_->input_textEdit->setEnabled(false);
   }
   virtual void applyRoomsFrameChange() override {
      // #new_logic : OTCShield? 
      // chat_->ui_->stackedWidgetOTC->setCurrentIndex(static_cast<int>(OTCPages::OTCGeneralRoomShieldPage));
   }
};

class PrivatePartyRequestedOutgoingState : public AbstractChatWidgetState {
public:
   explicit PrivatePartyRequestedOutgoingState(ChatWidget* chat) : AbstractChatWidgetState(chat) { enterState(); }
   virtual ~PrivatePartyRequestedOutgoingState() override = default;
protected:
   virtual void applyUserFrameChange() override {}
   virtual void applyChatFrameChange() override {
      chat_->ui_->textEditMessages->resetChatView();

      chat_->ui_->frameContactActions->setVisible(false);

      chat_->ui_->input_textEdit->setText({});
      chat_->ui_->input_textEdit->setVisible(true);
      chat_->ui_->input_textEdit->setEnabled(false);
   }
   virtual void applyRoomsFrameChange() override {
      // #new_logic : OTCShield? 
      // chat_->ui_->stackedWidgetOTC->setCurrentIndex(static_cast<int>(OTCPages::OTCGeneralRoomShieldPage));
   }
};

class PrivatePartyRequestedIncomingState : public AbstractChatWidgetState {
public:
   explicit PrivatePartyRequestedIncomingState(ChatWidget* chat) : AbstractChatWidgetState(chat) { enterState(); }
   virtual ~PrivatePartyRequestedIncomingState() override = default;
protected:
   virtual void applyUserFrameChange() override {}
   virtual void applyChatFrameChange() override {
      chat_->ui_->textEditMessages->resetChatView();

      chat_->ui_->pushButton_AcceptSend->setText(QObject::tr("ACCEPT"));
      chat_->ui_->pushButton_RejectCancel->setText(QObject::tr("REJECT"));
      chat_->ui_->frameContactActions->setVisible(true);

      chat_->ui_->input_textEdit->setText({});
      chat_->ui_->input_textEdit->setVisible(true);
      chat_->ui_->input_textEdit->setEnabled(false);
   }
   virtual void applyRoomsFrameChange() override {
      // #new_logic : OTCShield? 
      // chat_->ui_->stackedWidgetOTC->setCurrentIndex(static_cast<int>(OTCPages::OTCGeneralRoomShieldPage));
   }
};

class PrivatePartyRejectedState : public AbstractChatWidgetState {
public:
   explicit PrivatePartyRejectedState(ChatWidget* chat) : AbstractChatWidgetState(chat) { enterState(); }
   virtual ~PrivatePartyRejectedState() override = default;
protected:
   virtual void applyUserFrameChange() override {}
   virtual void applyChatFrameChange() override {
      chat_->ui_->textEditMessages->resetChatView();

      chat_->ui_->frameContactActions->setVisible(false);

      chat_->ui_->input_textEdit->setText({});
      chat_->ui_->input_textEdit->setVisible(true);
      chat_->ui_->input_textEdit->setEnabled(false);
   }
   virtual void applyRoomsFrameChange() override {
      // #new_logic : OTC shield?
   }
};


ChatWidget::ChatWidget(QWidget* parent)
   : QWidget(parent), ui_(new Ui::ChatWidget)
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
   ui_->frameContactActions->setVisible(false);
   ui_->stackedWidget->setCurrentIndex(1); //Basically stackedWidget should be removed

   //otcRequestViewModel_ = new OTCRequestViewModel(this);
   //ui_->treeViewOTCRequests->setModel(otcRequestViewModel_);

   qRegisterMetaType<std::vector<std::string>>();

   connect(ui_->pushButton_AcceptSend, &QPushButton::clicked, this, &ChatWidget::onContactRequestAcceptSendClicked);
   connect(ui_->pushButton_RejectCancel, &QPushButton::clicked, this, &ChatWidget::onContactRequestRejectCancelClicked);
}

void ChatWidget::init(const std::shared_ptr<ConnectionManager>& connectionManager,
   const std::shared_ptr<ApplicationSettings>& appSettings,
   const Chat::ChatClientServicePtr& chatClientServicePtr,
   const std::shared_ptr<spdlog::logger>& loggerPtr)
{
   loggerPtr_ = loggerPtr;

   chatClientServicePtr_ = chatClientServicePtr;

   installEventFilter(this);

   connect(chatClientServicePtr_.get(), &Chat::ChatClientService::clientLoggedOutFromServer, this, &ChatWidget::onLogout, Qt::QueuedConnection);
   connect(chatClientServicePtr_.get(), &Chat::ChatClientService::partyModelChanged, this, &ChatWidget::onPartyModelChanged, Qt::QueuedConnection);

   chatPartiesTreeModel_ = std::make_shared<ChatPartiesTreeModel>(chatClientServicePtr_);

   ChatPartiesSortProxyModelPtr charTreeSortModel = std::make_shared<ChatPartiesSortProxyModel>(chatPartiesTreeModel_);
   ui_->treeViewUsers->setModel(charTreeSortModel.get());
   ui_->treeViewUsers->sortByColumn(0, Qt::AscendingOrder);
   ui_->treeViewUsers->setSortingEnabled(true);
   ui_->treeViewUsers->setItemDelegate(new ChatClientUsersViewItemDelegate(charTreeSortModel, this));
   ui_->treeViewUsers->setActiveChatLabel(ui_->labelActiveChat);
   ui_->treeViewUsers->setChatClientServicePtr(chatClientServicePtr);

   // TODO: fix search widget
   ui_->searchWidget->init();
   ui_->searchWidget->clearLineEdit();
   ui_->searchWidget->setLineEditEnabled(false);
   ui_->searchWidget->setListVisible(false);

   ui_->textEditMessages->switchToChat("Global");

   changeState<ChatLogOutState>(); //Initial state is LoggedOut

   // connections
   // User actions
   connect(ui_->treeViewUsers, &QTreeView::clicked, this, &ChatWidget::onUserListClicked);
   connect(ui_->input_textEdit, &BSChatInput::sendMessage, this, &ChatWidget::onSendButtonClicked);
   connect(ui_->textEditMessages, &ChatMessagesTextEdit::messageRead, this, &ChatWidget::onMessageRead, Qt::QueuedConnection);

   connect(chatClientServicePtr_.get(), &Chat::ChatClientService::clientLoggedInToServer, this, &ChatWidget::onLogin, Qt::QueuedConnection);
   connect(chatClientServicePtr_.get(), &Chat::ChatClientService::clientLoggedOutFromServer, this, &ChatWidget::onLogout, Qt::QueuedConnection);
   connect(chatClientServicePtr_.get(), &Chat::ChatClientService::partyModelChanged, this, &ChatWidget::onPartyModelChanged, Qt::QueuedConnection);

   Chat::ClientPartyModelPtr chatModelPtr = chatClientServicePtr_->getClientPartyModelPtr();
   connect(chatModelPtr.get(), &Chat::ClientPartyModel::messageArrived, this, &ChatWidget::onSendArrived, Qt::QueuedConnection);
   connect(chatModelPtr.get(), &Chat::ClientPartyModel::clientPartyStatusChanged, this, &ChatWidget::onClientPartyStatusChanged, Qt::QueuedConnection);
   connect(chatModelPtr.get(), &Chat::ClientPartyModel::messageStateChanged, this, &ChatWidget::onMessageStateChanged, Qt::QueuedConnection);

   ui_->textEditMessages->setClientPartyModel(chatModelPtr);
   ui_->input_textEdit->setAcceptRichText(false);

/* TODO

   // Back end changes
   ui_->textEditMessages->setClientPartyModel(chatModelPtr);
   ui_->input_textEdit->setAcceptRichText(false);

   connect(ui_->input_textEdit, &BSChatInput::selectionChanged, this, &ChatWidget::onBSChatInputSelectionChanged);

   connect(ui_->textEditMessages, &QTextEdit::selectionChanged, this, &ChatWidget::onChatMessagesSelectionChanged);
   connect(ui_->textEditMessages, &ChatMessagesTextEdit::addContactRequired, this, &ChatWidget::onSendFriendRequest);
*/


/* OTC
   connect(ui_->treeViewOTCRequests->selectionModel(), &QItemSelectionModel::selectionChanged, this, &ChatWidget::OnOTCSelectionChanged);

   otcClient_ = new OtcClient(logger_, walletsMgr, armory, signContainer, this);
   connect(client_.get(), &ChatClient::contactConnected, otcClient_, &OtcClient::peerConnected);
   connect(client_.get(), &ChatClient::contactDisconnected, otcClient_, &OtcClient::peerDisconnected);
   connect(client_.get(), &ChatClient::otcMessageReceived, otcClient_, &OtcClient::processMessage);
   connect(otcClient_, &OtcClient::sendPbMessage, this, &ChatWidget::sendOtcPbMessage);

   connect(otcClient_, &OtcClient::sendMessage, client_.get(), &ChatClient::sendOtcMessage);

   connect(otcClient_, &OtcClient::peerUpdated, this, &ChatWidget::onOtcUpdated);

   connect(ui_->widgetNegotiateRequest, &OTCNegotiationRequestWidget::requestCreated, this, &ChatWidget::onOtcRequestSubmit);
   connect(ui_->widgetPullOwnOTCRequest, &PullOwnOTCRequestWidget::requestPulled, this, &ChatWidget::onOtcRequestPull);
   connect(ui_->widgetNegotiateResponse, &OTCNegotiationResponseWidget::responseAccepted, this, &ChatWidget::onOtcResponseAccept);
   connect(ui_->widgetNegotiateResponse, &OTCNegotiationResponseWidget::responseUpdated, this, &ChatWidget::onOtcResponseUpdate);
   connect(ui_->widgetNegotiateResponse, &OTCNegotiationResponseWidget::responseRejected, this, &ChatWidget::onOtcResponseReject);
*/
}

void ChatWidget::onContactRequestAcceptSendClicked()
{
}

void ChatWidget::onContactRequestRejectCancelClicked()
{
}

void ChatWidget::onPartyModelChanged()
{
   chatPartiesTreeModel_->partyModelChanged();
   ui_->treeViewUsers->expandAll();
}

void ChatWidget::onLogin()
{
   changeState<IdleState>();
}

void ChatWidget::onLogout()
{
   changeState<ChatLogOutState>();
}

void ChatWidget::processOtcPbMessage(const std::string& data)
{
}

void ChatWidget::onSendButtonClicked()
{
   return stateCurrent_->sendMessage();
}

void ChatWidget::onSendMessage()
{
   stateCurrent_->sendMessage();
}

void ChatWidget::onMessageRead(const std::string& partyId, const std::string& messageId)
{
   stateCurrent_->messageRead(partyId, messageId);
}

void ChatWidget::onSendArrived(const Chat::MessagePtrList& messagePtr)
{
   stateCurrent_->messageArrived(messagePtr);
}

void ChatWidget::onClientPartyStatusChanged(const Chat::ClientPartyPtr& clientPartyPtr)
{
   stateCurrent_->changePartyStatus(clientPartyPtr);
}

void ChatWidget::onMessageStateChanged(const std::string& partyId, const std::string& message_id, const int party_message_state)
{
   stateCurrent_->changeMessageState(partyId, message_id, party_message_state);
}

void ChatWidget::onUserListClicked(const QModelIndex& index)
{
   //save draft

   ChatPartiesSortProxyModel* chartProxyModel = static_cast<ChatPartiesSortProxyModel*>(ui_->treeViewUsers->model());
   PartyTreeItem* partyTreeItem = chartProxyModel->getInternalData(index);

   if (partyTreeItem->modelType() == UI::ElementType::Container) {
      currentChat_.clear();
      changeState<IdleState>();
      return;
   }

   const auto clientPartyPtr = partyTreeItem->data().value<Chat::ClientPartyPtr>();

   auto transitionChange = [this, clientPartyPtr]() 
   {
      currentChat_ = clientPartyPtr->id();
   };

   switch (clientPartyPtr->partyState())
   {
   case Chat::PartyState::UNINITIALIZED:
      changeState<PrivatePartyUninitState>(transitionChange);
      break;
   case Chat::PartyState::REQUESTED:
      if (clientPartyPtr->displayName() == chatClientServicePtr_->getClientPartyModelPtr()->ownUserName()) {
         changeState<PrivatePartyRequestedOutgoingState>(transitionChange);
      }
      else {
         changeState<PrivatePartyRequestedIncomingState>(transitionChange);
      }
      break;
   case Chat::PartyState::REJECTED:
      changeState<PrivatePartyRejectedState>(transitionChange);
      break;
   case Chat::PartyState::INITIALIZED:
      changeState<PrivatePartyInitState>(transitionChange);
      break;
   default:
      break;
   }
}