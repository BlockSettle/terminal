#include "ChatWidget.h"

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
#include <QApplication>
#include <QClipboard>

#include "BSChatInput.h"
#include "ChatClientUsersViewItemDelegate.h"
#include "ChatWidgetStates/ChatWidgetStates.h"
#include "OtcClient.h"
#include "OtcUtils.h"
#include "ui_ChatWidget.h"

using namespace bs::network;

ChatWidget::ChatWidget(QWidget* parent)
   : QWidget(parent), ui_(new Ui::ChatWidget)
{
   ui_->setupUi(this);

#ifndef Q_OS_WIN
   ui_->timeLabel->setMinimumSize(ui_->timeLabel->property("minimumSizeLinux").toSize());
#endif

   ui_->textEditMessages->onSetColumnsWidth(ui_->timeLabel->minimumWidth(),
      ui_->iconLabel->minimumWidth(),
      ui_->userLabel->minimumWidth(),
      ui_->messageLabel->minimumWidth());

   //Init UI and other stuff
   ui_->frameContactActions->setVisible(false);
   ui_->stackedWidget->setCurrentIndex(1); //Basically stackedWidget should be removed

   //otcRequestViewModel_ = new OTCRequestViewModel(this);
   //ui_->treeViewOTCRequests->setModel(otcRequestViewModel_);
   ui_->textEditMessages->viewport()->installEventFilter(this);
   ui_->input_textEdit->viewport()->installEventFilter(this);
   ui_->treeViewUsers->viewport()->installEventFilter(this);

   qRegisterMetaType<std::vector<std::string>>();
}

ChatWidget::~ChatWidget()
{
   // Should be done explicitly, since destructor for state could make changes inside chatWidget
   stateCurrent_.reset();
}

void ChatWidget::init(const std::shared_ptr<ConnectionManager>& connectionManager
   , const std::shared_ptr<ApplicationSettings>& appSettings
   , const Chat::ChatClientServicePtr& chatClientServicePtr
   , const std::shared_ptr<spdlog::logger>& loggerPtr
   , const std::shared_ptr<bs::sync::WalletsManager> &walletsMgr
   , const std::shared_ptr<ArmoryConnection> &armory
   , const std::shared_ptr<SignContainer> &signContainer)
{
   loggerPtr_ = loggerPtr;

   chatClientServicePtr_ = chatClientServicePtr;

   installEventFilter(this);

   connect(chatClientServicePtr_.get(), &Chat::ChatClientService::clientLoggedOutFromServer, this, &ChatWidget::onLogout, Qt::QueuedConnection);
   connect(chatClientServicePtr_.get(), &Chat::ChatClientService::partyModelChanged, this, &ChatWidget::onPartyModelChanged, Qt::QueuedConnection);
   connect(chatClientServicePtr_.get(), &Chat::ChatClientService::searchUserReply, ui_->searchWidget, &SearchWidget::onSearchUserReply);
   connect(ui_->searchWidget, &SearchWidget::showUserRoom, this, &ChatWidget::onShowUserRoom);
   connect(ui_->searchWidget, &SearchWidget::contactFriendRequest, this, &ChatWidget::onContactFriendRequest);

   chatPartiesTreeModel_ = std::make_shared<ChatPartiesTreeModel>(chatClientServicePtr_);

   ChatPartiesSortProxyModelPtr charTreeSortModel = std::make_shared<ChatPartiesSortProxyModel>(chatPartiesTreeModel_);
   ui_->treeViewUsers->setModel(charTreeSortModel.get());
   ui_->treeViewUsers->sortByColumn(0, Qt::AscendingOrder);
   ui_->treeViewUsers->setSortingEnabled(true);
   ui_->treeViewUsers->setItemDelegate(new ChatClientUsersViewItemDelegate(charTreeSortModel, this));
   ui_->treeViewUsers->setActiveChatLabel(ui_->labelActiveChat);

   ui_->searchWidget->init(chatClientServicePtr);
   ui_->searchWidget->onClearLineEdit();
   ui_->searchWidget->onSetLineEditEnabled(false);
   ui_->searchWidget->onSetListVisible(false);

   ui_->textEditMessages->onSwitchToChat("Global");

   changeState<ChatLogOutState>(); //Initial state is LoggedOut

   // connections
   // User actions
   connect(ui_->treeViewUsers, &ChatUserListTreeView::partyClicked, this, &ChatWidget::onUserListClicked);
   connect(ui_->treeViewUsers, &ChatUserListTreeView::removeFromContacts, this, &ChatWidget::onRemovePartyRequest);
   connect(ui_->treeViewUsers, &ChatUserListTreeView::acceptFriendRequest, this, &ChatWidget::onContactRequestAcceptClicked);
   connect(ui_->treeViewUsers, &ChatUserListTreeView::declineFriendRequest, this, &ChatWidget::onContactRequestRejectClicked);
   connect(ui_->treeViewUsers, &ChatUserListTreeView::setDisplayName, this, &ChatWidget::onSetDisplayName);

   connect(ui_->input_textEdit, &BSChatInput::sendMessage, this, &ChatWidget::onSendMessage);
   connect(ui_->textEditMessages, &ChatMessagesTextEdit::messageRead, this, &ChatWidget::onMessageRead, Qt::QueuedConnection);
   connect(ui_->textEditMessages, &ChatMessagesTextEdit::newPartyRequest, this, &ChatWidget::onNewPartyRequest, Qt::QueuedConnection);
   connect(ui_->textEditMessages, &ChatMessagesTextEdit::removePartyRequest, this, &ChatWidget::onRemovePartyRequest, Qt::QueuedConnection);
   connect(ui_->textEditMessages, &ChatMessagesTextEdit::switchPartyRequest, this, &ChatWidget::onActivatePartyId);

   connect(chatClientServicePtr_.get(), &Chat::ChatClientService::clientLoggedInToServer, this, &ChatWidget::onLogin, Qt::QueuedConnection);
   connect(chatClientServicePtr_.get(), &Chat::ChatClientService::clientLoggedOutFromServer, this, &ChatWidget::onLogout, Qt::QueuedConnection);
   connect(chatClientServicePtr_.get(), &Chat::ChatClientService::partyModelChanged, this, &ChatWidget::onPartyModelChanged, Qt::QueuedConnection);

   Chat::ClientPartyModelPtr clientPartyModelPtr = chatClientServicePtr_->getClientPartyModelPtr();
   connect(clientPartyModelPtr.get(), &Chat::ClientPartyModel::messageArrived, this, &ChatWidget::onSendArrived, Qt::QueuedConnection);
   connect(clientPartyModelPtr.get(), &Chat::ClientPartyModel::clientPartyStatusChanged, this, &ChatWidget::onClientPartyStatusChanged, Qt::QueuedConnection);
   connect(clientPartyModelPtr.get(), &Chat::ClientPartyModel::messageStateChanged, this, &ChatWidget::onMessageStateChanged, Qt::QueuedConnection);

   // Connect all signal that influence on widget appearance 
   connect(clientPartyModelPtr.get(), &Chat::ClientPartyModel::messageArrived, this, &ChatWidget::onRegisterNewChangingRefresh, Qt::QueuedConnection);
   connect(clientPartyModelPtr.get(), &Chat::ClientPartyModel::clientPartyStatusChanged, this, &ChatWidget::onRegisterNewChangingRefresh, Qt::QueuedConnection);
   connect(clientPartyModelPtr.get(), &Chat::ClientPartyModel::messageStateChanged, this, &ChatWidget::onRegisterNewChangingRefresh, Qt::QueuedConnection);

   ui_->textEditMessages->onSetClientPartyModel(clientPartyModelPtr);

   // OTC
   otcClient_ = new OtcClient(loggerPtr_, walletsMgr, armory, signContainer, this);
   connect(otcClient_, &OtcClient::sendPbMessage, this, &ChatWidget::sendOtcPbMessage);
   connect(otcClient_, &OtcClient::sendMessage, this, [this](const std::string &partyId, const BinaryData &data) {
      chatClientServicePtr_->SendPartyMessage(partyId, OtcUtils::serializeMessage(data));
   });
   connect(otcClient_, &OtcClient::peerUpdated, this, &ChatWidget::onOtcUpdated);

   connect(ui_->widgetNegotiateRequest, &OTCNegotiationRequestWidget::requestCreated, this, &ChatWidget::onOtcRequestSubmit);
   connect(ui_->widgetPullOwnOTCRequest, &PullOwnOTCRequestWidget::requestPulled, this, &ChatWidget::onOtcRequestPull);
   connect(ui_->widgetNegotiateResponse, &OTCNegotiationResponseWidget::responseAccepted, this, &ChatWidget::onOtcResponseAccept);
   connect(ui_->widgetNegotiateResponse, &OTCNegotiationResponseWidget::responseUpdated, this, &ChatWidget::onOtcResponseUpdate);
   connect(ui_->widgetNegotiateResponse, &OTCNegotiationResponseWidget::responseRejected, this, &ChatWidget::onOtcResponseReject);
}

void acceptPartyRequest(const std::string& partyId) {}
void rejectPartyRequest(const std::string& partyId) {}
void sendPartyRequest(const std::string& partyId) {}
void removePartyRequest(const std::string& partyId) {}

void ChatWidget::onContactRequestAcceptClicked(const std::string& partyId)
{
   stateCurrent_->onAcceptPartyRequest(partyId);
}

void ChatWidget::onContactRequestRejectClicked(const std::string& partyId)
{
   stateCurrent_->onRejectPartyRequest(partyId);
}

void ChatWidget::onContactRequestSendClicked(const std::string& partyId)
{
   stateCurrent_->onSendPartyRequest(partyId);
}

void ChatWidget::onContactRequestCancelClicked(const std::string& partyId)
{
   stateCurrent_->onRemovePartyRequest(partyId);
}

void ChatWidget::onNewPartyRequest(const std::string& userName)
{
   stateCurrent_->onNewPartyRequest(userName);
}

void ChatWidget::onRemovePartyRequest(const std::string& partyId)
{
   stateCurrent_->onRemovePartyRequest(partyId);
}

void ChatWidget::onOtcUpdated(const std::string &partyId)
{
   if (currentPartyId_ == partyId) {
      updateOtc();
   }
}

void ChatWidget::onPartyModelChanged()
{
   stateCurrent_->onResetPartyModel();
   // #new_logic : save expanding state
   ui_->treeViewUsers->expandAll();
}

void ChatWidget::onLogin()
{
   const auto clientPartyModelPtr = chatClientServicePtr_->getClientPartyModelPtr();
   ownUserId_ = clientPartyModelPtr->ownUserName();

   changeState<IdleState>();
   stateCurrent_->onResetPartyModel();
   ui_->treeViewUsers->expandAll();

   onActivatePartyId(ChatModelNames::PrivateTabGlobal);

   otcClient_->setCurrentUserId(ownUserId_);
}

void ChatWidget::onLogout()
{
   ownUserId_.clear();

   changeState<ChatLogOutState>();

   for (const auto &partyId : connectedPeers_) {
      otcClient_->peerDisconnected(partyId);
   }
   connectedPeers_.clear();
}

void ChatWidget::showEvent(QShowEvent* e)
{
   // refreshView
   if (bNeedRefresh_) {
      bNeedRefresh_ = false;
      onActivatePartyId(QString::fromStdString(currentPartyId_));
   }
}

bool ChatWidget::eventFilter(QObject* sender, QEvent* event)
{
   auto fClearSelection = [](QTextEdit* widget, bool bForce = false) {
      if (!widget->underMouse() || bForce) {
         QTextCursor textCursor = widget->textCursor();
         textCursor.clearSelection();
         widget->setTextCursor(textCursor);
      }
   };

   if ( QEvent::MouseButtonPress == event->type()) {
      fClearSelection(ui_->textEditMessages);
      fClearSelection(ui_->input_textEdit);
   }

   if (QEvent::KeyPress == event->type()) {
      QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);

      // handle ctrl+c (cmd+c on macOS)
      if (keyEvent->modifiers().testFlag(Qt::ControlModifier)) {
         if (Qt::Key_C == keyEvent->key()) {
            if (ui_->textEditMessages->textCursor().hasSelection()) {
               QApplication::clipboard()->setText(ui_->textEditMessages->getFormattedTextFromSelection());
               fClearSelection(ui_->textEditMessages, true);
               return true;
            }
         }
      }
      if (keyEvent->matches(QKeySequence::SelectAll)) {
         fClearSelection(ui_->textEditMessages);
      }
   }

   return QWidget::eventFilter(sender, event);
}

void ChatWidget::processOtcPbMessage(const std::string& data)
{
   otcClient_->processPbMessage(data);
}

void ChatWidget::onConnectedToServer()
{
   otcClient_->setCurrentUserId(ownUserId_);
}

void ChatWidget::onNewChatMessageTrayNotificationClicked(const QString& partyId)
{
   onActivatePartyId(partyId);
}

void ChatWidget::onSendMessage()
{
   stateCurrent_->onSendMessage();
}

void ChatWidget::onMessageRead(const std::string& partyId, const std::string& messageId)
{
   stateCurrent_->onMessageRead(partyId, messageId);
}

void ChatWidget::onSendArrived(const Chat::MessagePtrList& messagePtr)
{
   stateCurrent_->onProcessMessageArrived(messagePtr);

   for (const auto &msg : messagePtr) {
      if (msg->partyMessageState() == Chat::SENT) {
         auto data = OtcUtils::deserializeMessage(msg->messageText());
         if (!data.isNull()) {
            otcClient_->processMessage(msg->partyId(), data);
         }
      }
   }
}

void ChatWidget::onClientPartyStatusChanged(const Chat::ClientPartyPtr& clientPartyPtr)
{
   stateCurrent_->onChangePartyStatus(clientPartyPtr);

   if (clientPartyPtr->partyType() == Chat::PRIVATE_DIRECT_MESSAGE) {
      const std::string &partyId = clientPartyPtr->id();
      bool wasConnected = connectedPeers_.find(clientPartyPtr->id()) != connectedPeers_.end();

      if (clientPartyPtr->clientStatus() == Chat::ONLINE && !wasConnected) {
         otcClient_->peerConnected(partyId);
         connectedPeers_.insert(partyId);
      }

      if (clientPartyPtr->clientStatus() == Chat::OFFLINE && wasConnected) {
         otcClient_->peerDisconnected(partyId);
         connectedPeers_.erase(partyId);
      }
   }
}

void ChatWidget::onMessageStateChanged(const std::string& partyId, const std::string& message_id, const int party_message_state)
{
   stateCurrent_->onChangeMessageState(partyId, message_id, party_message_state);
}

void ChatWidget::onUserListClicked(const QModelIndex& index)
{
   if (!index.isValid()) {
      return;
   }

   ChatPartiesSortProxyModel* chartProxyModel = static_cast<ChatPartiesSortProxyModel*>(ui_->treeViewUsers->model());
   PartyTreeItem* partyTreeItem = chartProxyModel->getInternalData(index);

   if (partyTreeItem->modelType() == UI::ElementType::Container) {
      return;
   }

   const auto clientPartyPtr = partyTreeItem->data().value<Chat::ClientPartyPtr>();
   chatTransition(clientPartyPtr);
}

void ChatWidget::chatTransition(const Chat::ClientPartyPtr& clientPartyPtr)
{
   auto transitionChange = [this, clientPartyPtr]()
   {
      currentPartyId_ = clientPartyPtr->id();
   };

   switch (clientPartyPtr->partyState())
   {
   case Chat::PartyState::UNINITIALIZED:
      changeState<PrivatePartyUninitState>(transitionChange);
      break;
   case Chat::PartyState::REQUESTED:
      if (clientPartyPtr->partyCreatorHash() == ownUserId_) {
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

   updateOtc();
}

void ChatWidget::updateOtc()
{
   auto peer = otcClient_->peer(currentPartyId_);
   if (!peer) {
      ui_->stackedWidgetOTC->setCurrentIndex(int(OTCPages::OTCLoginRequiredShieldPage));
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

void ChatWidget::onActivatePartyId(const QString& partyId)
{
   ChatPartiesSortProxyModel* chartProxyModel = static_cast<ChatPartiesSortProxyModel*>(ui_->treeViewUsers->model());
   const QModelIndex partyProxyIndex = chartProxyModel->getProxyIndexById(partyId.toStdString());
   if (!partyProxyIndex.isValid()) {
      if (ownUserId_.empty()) {
         currentPartyId_.clear();
         changeState<ChatLogOutState>();
      }
      else {
         Q_ASSERT(partyId != ChatModelNames::PrivateTabGlobal);
         onActivatePartyId(ChatModelNames::PrivateTabGlobal);
      }
      return;
   }

   ui_->treeViewUsers->setCurrentIndex(partyProxyIndex);
   onUserListClicked(partyProxyIndex);
}

void ChatWidget::onRegisterNewChangingRefresh()
{
   if (!isVisible() || !isActiveWindow()) {
      bNeedRefresh_ = true;
   }
}

void ChatWidget::onShowUserRoom(const QString& userHash)
{
   const auto clientPartyModelPtr = chatClientServicePtr_->getClientPartyModelPtr();
   Chat::ClientPartyPtr clientPartyPtr = clientPartyModelPtr->getPartyByUserName(userHash.toStdString());

   if (!clientPartyPtr) {
      return;
   }

   chatTransition(clientPartyPtr);
}

void ChatWidget::onContactFriendRequest(const QString& userHash)
{
   chatClientServicePtr_->RequestPrivateParty(userHash.toStdString());
}

void ChatWidget::onSetDisplayName(const std::string& partyId, const std::string& contactName)
{
   stateCurrent_->onUpdateDisplayName(partyId, contactName);
}

void ChatWidget::onOtcRequestSubmit()
{
   bool result = otcClient_->sendOffer(ui_->widgetNegotiateRequest->offer(), currentPartyId_);
   if (!result) {
      SPDLOG_LOGGER_ERROR(loggerPtr_, "send offer failed");
      return;
   }
}

void ChatWidget::onOtcRequestPull()
{
   bool result = otcClient_->pullOrRejectOffer(currentPartyId_);
   if (!result) {
      SPDLOG_LOGGER_ERROR(loggerPtr_, "pull offer failed");
      return;
   }
}

void ChatWidget::onOtcResponseAccept()
{
   bool result = otcClient_->acceptOffer(ui_->widgetNegotiateResponse->offer(), currentPartyId_);
   if (!result) {
      SPDLOG_LOGGER_ERROR(loggerPtr_, "accept offer failed");
      return;
   }
}

void ChatWidget::onOtcResponseUpdate()
{
   bool result = otcClient_->updateOffer(ui_->widgetNegotiateResponse->offer(), currentPartyId_);
   if (!result) {
      SPDLOG_LOGGER_ERROR(loggerPtr_, "update offer failed");
      return;
   }
}

void ChatWidget::onOtcResponseReject()
{
   bool result = otcClient_->pullOrRejectOffer(currentPartyId_);
   if (!result) {
      SPDLOG_LOGGER_ERROR(loggerPtr_, "reject offer failed");
      return;
   }
}
