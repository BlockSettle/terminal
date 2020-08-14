/*

***********************************************************************************
* Copyright (C) 2019 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "ChatWidget.h"

#include <spdlog/spdlog.h>

#include <QClipboard>
#include <QFileDialog>
#include <QUrl>

#include "BSChatInput.h"
#include "BSMessageBox.h"
#include "ImportKeyBox.h"
#include "ChatClientUsersViewItemDelegate.h"
#include "ChatWidgetStates/ChatWidgetStates.h"
#include "ChatOTCHelper.h"
#include "OtcUtils.h"
#include "OtcClient.h"
#include "OTCRequestViewModel.h"
#include "OTCShieldWidgets/OTCWindowsManager.h"
#include "AuthAddressManager.h"
#include "MDCallbacksQt.h"
#include "AssetManager.h"
#include "ui_ChatWidget.h"
#include "UtxoReservationManager.h"
#include "ApplicationSettings.h"
#include "chat.pb.h"

using namespace bs;
using namespace bs::network;

namespace
{
   const QString showHistoryButtonName = QObject::tr("Show History");
}

ChatWidget::ChatWidget(QWidget* parent)
   : QWidget(parent), ui_(new Ui::ChatWidget)
{
   qRegisterMetaType<std::vector<std::string>>();
   qRegisterMetaType<Chat::UserPublicKeyInfoPtr>();
   qRegisterMetaType<Chat::UserPublicKeyInfoList>();
   qRegisterMetaType<Chat::PartyModelError>();

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

   ui_->textEditMessages->viewport()->installEventFilter(this);
   ui_->input_textEdit->viewport()->installEventFilter(this);
   ui_->treeViewUsers->viewport()->installEventFilter(this);

   ui_->showHistoryButton->setText(showHistoryButtonName);
   ui_->showHistoryButton->setVisible(false);

   otcWindowsManager_ = std::make_shared<OTCWindowsManager>();
   auto* sWidget = ui_->stackedWidgetOTC;
   for (int index = 0; index < sWidget->count(); ++index) {
      auto* widget = qobject_cast<OTCWindowsAdapterBase*>(sWidget->widget(index));
      if (widget) {
         widget->setChatOTCManager(otcWindowsManager_);
         connect(this, &ChatWidget::chatRoomChanged, widget, &OTCWindowsAdapterBase::onChatRoomChanged);
         connect(this, &ChatWidget::onAboutToHide, widget, &OTCWindowsAdapterBase::onParentAboutToHide);
      }
   }
   connect(otcWindowsManager_.get(), &OTCWindowsManager::syncInterfaceRequired, this, &ChatWidget::onUpdateOTCShield);

   changeState<ChatLogOutState>(); //Initial state is LoggedOut
}

ChatWidget::~ChatWidget()
{
   // Should be done explicitly, since destructor for state could make changes inside chatWidget
   stateCurrent_.reset();
}

void ChatWidget::init(const std::shared_ptr<ConnectionManager>& connectionManager
   , bs::network::otc::Env env
   , const Chat::ChatClientServicePtr& chatClientServicePtr
   , const std::shared_ptr<spdlog::logger>& loggerPtr
   , const std::shared_ptr<bs::sync::WalletsManager>& walletsMgr
   , const std::shared_ptr<AuthAddressManager> &authManager
   , const std::shared_ptr<ArmoryConnection>& armory
   , const std::shared_ptr<WalletSignerContainer>& signContainer
   , const std::shared_ptr<MDCallbacksQt> &mdCallbacks
   , const std::shared_ptr<AssetManager>& assetManager
   , const std::shared_ptr<bs::UTXOReservationManager> &utxoReservationManager
   , const std::shared_ptr<ApplicationSettings>& applicationSettings)
{
   loggerPtr_ = loggerPtr;

   // OTC
   otcHelper_ = new ChatOTCHelper(this);
   otcHelper_->init(env, loggerPtr, walletsMgr, armory, signContainer,
      authManager, utxoReservationManager, applicationSettings);
   otcWindowsManager_->init(walletsMgr, authManager, mdCallbacks, assetManager
      , armory, utxoReservationManager);

   chatClientServicePtr_ = chatClientServicePtr;

   installEventFilter(this);

   connect(chatClientServicePtr_.get(), &Chat::ChatClientService::clientLoggedOutFromServer, this, &ChatWidget::onLogout, Qt::QueuedConnection);
   connect(chatClientServicePtr_.get(), &Chat::ChatClientService::partyModelChanged, this, &ChatWidget::onPartyModelChanged, Qt::QueuedConnection);
   connect(chatClientServicePtr_.get(), &Chat::ChatClientService::searchUserReply, ui_->searchWidget, &SearchWidget::onSearchUserReply);
   connect(ui_->searchWidget, &SearchWidget::showUserRoom, this, &ChatWidget::onShowUserRoom);
   connect(ui_->searchWidget, &SearchWidget::contactFriendRequest, this, &ChatWidget::onContactFriendRequest);
   connect(ui_->searchWidget, &SearchWidget::emailHashRequested, this, &ChatWidget::emailHashRequested);

   chatPartiesTreeModel_ = std::make_shared<ChatPartiesTreeModel>(chatClientServicePtr_, otcHelper_->client());

   const ChatPartiesSortProxyModelPtr charTreeSortModel = std::make_shared<ChatPartiesSortProxyModel>(chatPartiesTreeModel_);
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

   connect(ui_->showHistoryButton, &QPushButton::pressed, this, &ChatWidget::onRequestAllPrivateMessages);


   ui_->widgetOTCShield->init(walletsMgr, authManager, applicationSettings);
   connect(ui_->widgetOTCShield, &OTCShield::requestPrimaryWalletCreation, this, &ChatWidget::requestPrimaryWalletCreation);

   // connections
   // User actions
   connect(ui_->treeViewUsers, &ChatUserListTreeView::partyClicked, this, &ChatWidget::onUserListClicked);
   connect(ui_->treeViewUsers, &ChatUserListTreeView::removeFromContacts, this, &ChatWidget::onRemovePartyRequest);
   connect(ui_->treeViewUsers, &ChatUserListTreeView::acceptFriendRequest, this, &ChatWidget::onContactRequestAcceptClicked);
   connect(ui_->treeViewUsers, &ChatUserListTreeView::declineFriendRequest, this, &ChatWidget::onContactRequestRejectClicked);
   connect(ui_->treeViewUsers, &ChatUserListTreeView::setDisplayName, this, &ChatWidget::onSetDisplayName);
   // This should be queued connection to make sure first view is updated
   connect(chatPartiesTreeModel_.get(), &ChatPartiesTreeModel::restoreSelectedIndex, this, &ChatWidget::onActivateCurrentPartyId, Qt::QueuedConnection);

   connect(ui_->input_textEdit, &BSChatInput::sendMessage, this, &ChatWidget::onSendMessage);
   connect(ui_->textEditMessages, &ChatMessagesTextEdit::messageRead, this, &ChatWidget::onMessageRead);
   connect(ui_->textEditMessages, &ChatMessagesTextEdit::newPartyRequest, this, &ChatWidget::onNewPartyRequest);
   connect(ui_->textEditMessages, &ChatMessagesTextEdit::removePartyRequest, this, &ChatWidget::onRemovePartyRequest);
   connect(ui_->textEditMessages, &ChatMessagesTextEdit::switchPartyRequest, this, &ChatWidget::onActivatePartyId);

   //connect(chatClientServicePtr_.get(), &Chat::ChatClientService::clientLoggedInToServer, this, &ChatWidget::onLogin, Qt::QueuedConnection);
   connect(chatClientServicePtr_.get(), &Chat::ChatClientService::clientLoggedInToServer, this, &ChatWidget::onLogin);
   connect(chatClientServicePtr_.get(), &Chat::ChatClientService::clientLoggedOutFromServer, this, &ChatWidget::onLogout, Qt::QueuedConnection);
   connect(chatClientServicePtr_.get(), &Chat::ChatClientService::partyModelChanged, this, &ChatWidget::onPartyModelChanged, Qt::QueuedConnection);
   connect(chatClientServicePtr_.get(), &Chat::ChatClientService::privateMessagesHistoryCount, this, &ChatWidget::onPrivateMessagesHistoryCount, Qt::QueuedConnection);

   const Chat::ClientPartyModelPtr clientPartyModelPtr = chatClientServicePtr_->getClientPartyModelPtr();
   connect(clientPartyModelPtr.get(), &Chat::ClientPartyModel::messageArrived, this, &ChatWidget::onSendArrived, Qt::QueuedConnection);
   connect(clientPartyModelPtr.get(), &Chat::ClientPartyModel::clientPartyStatusChanged, this, &ChatWidget::onClientPartyStatusChanged, Qt::QueuedConnection);
   connect(clientPartyModelPtr.get(), &Chat::ClientPartyModel::messageStateChanged, this, &ChatWidget::onMessageStateChanged, Qt::QueuedConnection);

   // Connect all signal that influence on widget appearance
   connect(clientPartyModelPtr.get(), &Chat::ClientPartyModel::messageArrived, this, &ChatWidget::onRegisterNewChangingRefresh, Qt::QueuedConnection);
   connect(clientPartyModelPtr.get(), &Chat::ClientPartyModel::clientPartyStatusChanged, this, &ChatWidget::onRegisterNewChangingRefresh, Qt::QueuedConnection);
   connect(clientPartyModelPtr.get(), &Chat::ClientPartyModel::messageStateChanged, this, &ChatWidget::onRegisterNewChangingRefresh, Qt::QueuedConnection);

   // OTC
   connect(clientPartyModelPtr.get(), &Chat::ClientPartyModel::otcPrivatePartyReady, this, &ChatWidget::onOtcPrivatePartyReady, Qt::QueuedConnection);

   ui_->textEditMessages->onSetClientPartyModel(clientPartyModelPtr);

   otcRequestViewModel_ = new OTCRequestViewModel(otcHelper_->client(), this);
   ui_->treeViewOTCRequests->setModel(otcRequestViewModel_);
   connect(ui_->treeViewOTCRequests->selectionModel(), &QItemSelectionModel::currentChanged, this, &ChatWidget::onOtcRequestCurrentChanged);
   connect(chatPartiesTreeModel_.get(), &ChatPartiesTreeModel::restoreSelectedIndex, this, &ChatWidget::onActivateGlobalOTCTableRow, Qt::QueuedConnection);

   connect(otcHelper_->client(), &OtcClient::sendPbMessage, this, &ChatWidget::sendOtcPbMessage);
   connect(otcHelper_->client(), &OtcClient::sendContactMessage, this, &ChatWidget::onSendOtcMessage);
   connect(otcHelper_->client(), &OtcClient::sendPublicMessage, this, &ChatWidget::onSendOtcPublicMessage);
   connect(otcHelper_->client(), &OtcClient::peerUpdated, this, &ChatWidget::onOtcUpdated);
   connect(otcHelper_->client(), &OtcClient::publicUpdated, this, &ChatWidget::onOtcPublicUpdated);
   connect(otcHelper_->client(), &OtcClient::publicUpdated, otcRequestViewModel_, &OTCRequestViewModel::onRequestsUpdated);
   connect(otcHelper_->client(), &OtcClient::peerError, this, &ChatWidget::onOTCPeerError);


   connect(ui_->widgetNegotiateRequest, &OTCNegotiationRequestWidget::requestCreated, this, &ChatWidget::onOtcRequestSubmit);
   connect(ui_->widgetPullOwnOTCRequest, &PullOwnOTCRequestWidget::currentRequestPulled, this, &ChatWidget::onOtcPullOrRejectCurrent);
   connect(ui_->widgetNegotiateResponse, &OTCNegotiationResponseWidget::responseAccepted, this, &ChatWidget::onOtcResponseAccept);
   connect(ui_->widgetNegotiateResponse, &OTCNegotiationResponseWidget::responseUpdated, this, &ChatWidget::onOtcResponseUpdate);
   connect(ui_->widgetNegotiateResponse, &OTCNegotiationResponseWidget::responseRejected, this, &ChatWidget::onOtcPullOrRejectCurrent);
   connect(ui_->widgetCreateOTCRequest, &CreateOTCRequestWidget::requestCreated, this, &ChatWidget::onOtcQuoteRequestSubmit);
   connect(ui_->widgetCreateOTCResponse, &CreateOTCResponseWidget::responseCreated, this, &ChatWidget::onOtcQuoteResponseSubmit);

   ui_->widgetCreateOTCRequest->init(env);
}

otc::PeerPtr ChatWidget::currentPeer() const
{
   const auto chartProxyModel = dynamic_cast<ChatPartiesSortProxyModel*>(ui_->treeViewUsers->model());
   PartyTreeItem* partyTreeItem = chartProxyModel->getInternalData(ui_->treeViewUsers->currentIndex());
   if (!partyTreeItem || partyTreeItem->modelType() == bs::UI::ElementType::Container) {
      return nullptr;
   }

   const auto clientPartyPtr = partyTreeItem->data().value<Chat::ClientPartyPtr>();
   if (!clientPartyPtr || clientPartyPtr->isGlobalStandard()) {
      return nullptr;
   }

   if (clientPartyPtr->isGlobalOTC()) {
      const auto &currentIndex = ui_->treeViewOTCRequests->selectionModel()->currentIndex();
      if (!currentIndex.isValid() || currentIndex.row() < 0 || currentIndex.row() >= int(otcHelper_->client()->requests().size())) {
         // Show by default own request (if available)
         return otcHelper_->client()->ownRequest();
      }

      return otcHelper_->client()->requests().at(size_t(currentIndex.row()));
   }

   return otcHelper_->client()->peer(clientPartyPtr->userHash(), partyTreeItem->peerType);
}

void ChatWidget::setUserType(UserType userType)
{
   userType_ = userType;
}

void acceptPartyRequest(const std::string& partyId) {}
void rejectPartyRequest(const std::string& partyId) {}
void sendPartyRequest(const std::string& partyId) {}
void removePartyRequest(const std::string& partyId) {}

void ChatWidget::onContactRequestAcceptClicked(const std::string& partyId) const
{
   stateCurrent_->onAcceptPartyRequest(partyId);
}

void ChatWidget::onContactRequestRejectClicked(const std::string& partyId) const
{
   stateCurrent_->onRejectPartyRequest(partyId);
}

void ChatWidget::onContactRequestSendClicked(const std::string& partyId) const
{
   stateCurrent_->onSendPartyRequest(partyId);
}

void ChatWidget::onContactRequestCancelClicked(const std::string& partyId) const
{
   stateCurrent_->onRemovePartyRequest(partyId);
}

void ChatWidget::onNewPartyRequest(const std::string& userName, const std::string& initialMessage) const
{
   stateCurrent_->onNewPartyRequest(userName, initialMessage);
}

void ChatWidget::onRemovePartyRequest(const std::string& partyId) const
{
   stateCurrent_->onRemovePartyRequest(partyId);
}

void ChatWidget::onOtcUpdated(const otc::PeerPtr &peer)
{
   stateCurrent_->onOtcUpdated(peer);
}

void ChatWidget::onOtcPublicUpdated() const
{
   stateCurrent_->onOtcPublicUpdated();
   ui_->treeViewUsers->onExpandGlobalOTC();
}

void ChatWidget::onOTCPeerError(const otc::PeerPtr &peer, bs::network::otc::PeerErrorType type, const std::string* errorMsg)
{
   stateCurrent_->onOTCPeerError(peer, type, errorMsg);
}

void ChatWidget::onUpdateOTCShield() const
{
   stateCurrent_->onUpdateOTCShield();
}

void ChatWidget::onEmailHashReceived(const std::string &email, const std::string &hash) const
{
   ui_->searchWidget->onEmailHashReceived(email, hash);
}

void ChatWidget::onPartyModelChanged() const
{
   stateCurrent_->onResetPartyModel();
   // #new_logic : save expanding state
   ui_->treeViewUsers->expandAll();
}

void ChatWidget::onLogin()
{
   const auto clientPartyModelPtr = chatClientServicePtr_->getClientPartyModelPtr();
   ownUserId_ = clientPartyModelPtr->ownUserName();
   otcHelper_->setCurrentUserId(ownUserId_);

   changeState<IdleState>();
   stateCurrent_->onResetPartyModel();
   ui_->treeViewUsers->expandAll();

   onActivatePartyId(QString::fromLatin1(Chat::GlobalRoomName));
}

void ChatWidget::onLogout()
{
   ownUserId_.clear();
   changeState<ChatLogOutState>();
}

void ChatWidget::showEvent(QShowEvent* e)
{
   // refreshView
   if (bNeedRefresh_) {
      bNeedRefresh_ = false;
      onActivatePartyId(QString::fromStdString(currentPartyId_));
   }
}

void ChatWidget::hideEvent(QHideEvent* event)
{
   emit onAboutToHide();
   QWidget::hideEvent(event);
}

bool ChatWidget::eventFilter(QObject* sender, QEvent* event)
{
   const auto fClearSelection = [](QTextEdit* widget, bool bForce = false) {
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
      auto*keyEvent = dynamic_cast<QKeyEvent *>(event);

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

void ChatWidget::onProcessOtcPbMessage(const Blocksettle::Communication::ProxyTerminalPb::Response &response) const
{
   stateCurrent_->onProcessOtcPbMessage(response);
}

void ChatWidget::onSendOtcMessage(const std::string& contactId, const BinaryData& data) const
{
   auto* chatProxyModel = dynamic_cast<ChatPartiesSortProxyModel*>(ui_->treeViewUsers->model());
   assert(chatProxyModel);

   const QModelIndex index = chatProxyModel->getProxyIndexById(currentPartyId_);
   const auto party = chatProxyModel->getInternalData(index);
   assert(party);

   bool const isOTCGlobalRoot = (chatProxyModel->getOTCGlobalRoot() == index);

   Chat::ClientPartyPtr clientPartyPtr = nullptr;
   if (party->peerType == bs::network::otc::PeerType::Contact && !isOTCGlobalRoot) {
      clientPartyPtr = chatClientServicePtr_->getClientPartyModelPtr()->getStandardPartyForUsers(ownUserId_, contactId);
   }
   else {
      clientPartyPtr = chatClientServicePtr_->getClientPartyModelPtr()->getOtcPartyForUsers(ownUserId_, contactId);
   }

   if (!clientPartyPtr) {
      SPDLOG_LOGGER_ERROR(loggerPtr_, "can't find valid private party to send OTC message");
      return;
   }
   stateCurrent_->onSendOtcMessage(clientPartyPtr->id(), OtcUtils::serializeMessage(data));
}

void ChatWidget::onSendOtcPublicMessage(const BinaryData &data) const
{
   stateCurrent_->onSendOtcPublicMessage(OtcUtils::serializePublicMessage(data));
}

void ChatWidget::onNewChatMessageTrayNotificationClicked(const QString& partyId)
{
   onActivatePartyId(partyId);
}

void ChatWidget::onSendMessage() const
{
   stateCurrent_->onSendMessage();
}

void ChatWidget::onMessageRead(const std::string& partyId, const std::string& messageId) const
{
   stateCurrent_->onMessageRead(partyId, messageId);
}

void ChatWidget::onSendArrived(const Chat::MessagePtrList& messagePtrList) const
{
   stateCurrent_->onProcessMessageArrived(messagePtrList);

   const auto clientPartyModelPtr = chatClientServicePtr_->getClientPartyModelPtr();
   for (const auto& message : messagePtrList)
   {
      const auto privatePartyPtr = clientPartyModelPtr->getPrivatePartyById(message->partyId());
      if (privatePartyPtr && privatePartyPtr->id() == currentPartyId_ && privatePartyPtr->isPrivateStandard())
      {
         // request only for private standard parties
         // and only once per loop
         chatClientServicePtr_->RequestPrivateMessagesHistoryCount(privatePartyPtr->id());
         return;
      }
   }
}

void ChatWidget::onClientPartyStatusChanged(const Chat::ClientPartyPtr& clientPartyPtr) const
{
   stateCurrent_->onChangePartyStatus(clientPartyPtr);
}

void ChatWidget::onMessageStateChanged(const std::string& partyId, const std::string& message_id, const int party_message_state) const
{
   stateCurrent_->onChangeMessageState(partyId, message_id, party_message_state);
}

void ChatWidget::onUserListClicked(const QModelIndex& index)
{
   if (!index.isValid()) {
      return;
   }

   auto* chartProxyModel = dynamic_cast<ChatPartiesSortProxyModel*>(ui_->treeViewUsers->model());
   PartyTreeItem* partyTreeItem = chartProxyModel->getInternalData(index);

   if (partyTreeItem->modelType() == bs::UI::ElementType::Container) {
      return;
   }

   const auto clientPartyPtr = partyTreeItem->data().value<Chat::ClientPartyPtr>();
   chatTransition(clientPartyPtr);
}

void ChatWidget::chatTransition(const Chat::ClientPartyPtr& clientPartyPtr)
{
   ui_->showHistoryButton->setVisible(false);

   const auto transitionChange = [this, clientPartyPtr]() {
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
   case Chat::PartyState::INITIALIZED:
      changeState<PrivatePartyInitState>(transitionChange);
      if (clientPartyPtr->isPrivateStandard()) {
         chatClientServicePtr_->RequestPrivateMessagesHistoryCount(clientPartyPtr->id());
      }
      break;
   default:
      break;
   }

   emit chatRoomChanged();
}

void ChatWidget::onActivatePartyId(const QString& partyId)
{
   auto* chartProxyModel = dynamic_cast<ChatPartiesSortProxyModel*>(ui_->treeViewUsers->model());
   const QModelIndex partyProxyIndex = chartProxyModel->getProxyIndexById(partyId.toStdString());
   if (!partyProxyIndex.isValid()) {
      if (ownUserId_.empty()) {
         currentPartyId_.clear();
         changeState<ChatLogOutState>();
      }
      else {
         Q_ASSERT(partyId != QString::fromLatin1(Chat::GlobalRoomName));
         onActivatePartyId(QString::fromLatin1(Chat::GlobalRoomName));
      }
      return;
   }

   ui_->treeViewUsers->setCurrentIndex(partyProxyIndex);
   onUserListClicked(partyProxyIndex);
}

void ChatWidget::onActivateGlobalPartyId()
{
   onActivatePartyId(QString::fromLatin1(Chat::GlobalRoomName));
}

void ChatWidget::onActivateCurrentPartyId()
{
   if (currentPartyId_.empty()) {
      return;
   }

   auto* chatProxyModel = dynamic_cast<ChatPartiesSortProxyModel*>(ui_->treeViewUsers->model());
   Q_ASSERT(chatProxyModel);

   const QModelIndex index = chatProxyModel->getProxyIndexById(currentPartyId_);
   if (!index.isValid()) {
      onActivateGlobalPartyId();
   }

   if (ui_->treeViewUsers->selectionModel()->currentIndex() != index) {
      ui_->treeViewUsers->selectionModel()->setCurrentIndex(index, QItemSelectionModel::Select);
   }
}

void ChatWidget::onActivateGlobalOTCTableRow() const
{
   const QDateTime timeStamp = otcHelper_->selectedGlobalOTCEntryTimeStamp();

   if (!timeStamp.isValid()) {
      return;
   }

   const QModelIndex currentRequest = otcRequestViewModel_->getIndexByTimestamp(timeStamp);

   if (!currentRequest.isValid()) {
      return;
   }

   ui_->treeViewOTCRequests->setCurrentIndex(currentRequest);
   stateCurrent_->onUpdateOTCShield();
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

   const Chat::ClientPartyPtr clientPartyPtr = clientPartyModelPtr->getStandardPartyForUsers(ownUserId_, userHash.toStdString());

   if (!clientPartyPtr) {
      return;
   }

   chatTransition(clientPartyPtr);
}

void ChatWidget::onContactFriendRequest(const QString& userHash) const
{
   ui_->textEditMessages->onShowRequestPartyBox(userHash.toStdString());
}

void ChatWidget::onSetDisplayName(const std::string& partyId, const std::string& contactName) const
{
   stateCurrent_->onUpdateDisplayName(partyId, contactName);
}

void ChatWidget::onOtcRequestSubmit() const
{
   stateCurrent_->onOtcRequestSubmit();
}

void ChatWidget::onOtcResponseAccept() const
{
   stateCurrent_->onOtcResponseAccept();
}

void ChatWidget::onOtcResponseUpdate() const
{
   stateCurrent_->onOtcResponseUpdate();
}

void ChatWidget::onOtcQuoteRequestSubmit() const
{
   stateCurrent_->onOtcQuoteRequestSubmit();
}

void ChatWidget::onOtcQuoteResponseSubmit() const
{
   stateCurrent_->onOtcQuoteResponseSubmit();
}

void ChatWidget::onOtcPullOrRejectCurrent() const
{
   stateCurrent_->onOtcPullOrRejectCurrent();
}

void ChatWidget::onUserPublicKeyChanged(const Chat::UserPublicKeyInfoList& userPublicKeyInfoList)
{
   // only one key needs to be replaced - show one message box
   if (userPublicKeyInfoList.size() == 1)
   {
      onConfirmContactNewKeyData(userPublicKeyInfoList, false);
      return;
   }

   // multiple keys replacing
   const QString detailsPattern = tr("Contacts Require key update: %1");

   const QString  detailsString = detailsPattern.arg(userPublicKeyInfoList.size());

   BSMessageBox bsMessageBox(BSMessageBox::question, tr("Contacts Information Update"),
      tr("Do you wish to import your full Contact list?"),
      tr("Press OK to Import all Contact ID keys. Selecting Cancel will allow you to determine each contact individually."),
      detailsString);
   const int ret = bsMessageBox.exec();

   onConfirmContactNewKeyData(userPublicKeyInfoList, QDialog::Accepted == ret);
}

void ChatWidget::onConfirmContactNewKeyData(const Chat::UserPublicKeyInfoList& userPublicKeyInfoList, bool bForceUpdateAllUsers)
{
   Chat::UserPublicKeyInfoList acceptList;
   Chat::UserPublicKeyInfoList declineList;

   for (const auto& userPkPtr : userPublicKeyInfoList)
   {
      if (userPkPtr->newPublicKey() == userPkPtr->oldPublicKey()) {
         continue;
      }

      if (bForceUpdateAllUsers)
      {
         acceptList.push_back(userPkPtr);
         continue;
      }

      ImportKeyBox box(BSMessageBox::question, tr("Import Contact '%1' Public Key?").arg(userPkPtr->user_hash()), this);
      box.setAddrPort(std::string());
      box.setNewKeyFromBinary(userPkPtr->newPublicKey());
      box.setOldKeyFromBinary(userPkPtr->oldPublicKey());
      box.setCancelVisible(true);

      if (box.exec() == QDialog::Accepted)
      {
         acceptList.push_back(userPkPtr);
         continue;
      }

      declineList.push_back(userPkPtr);
   }

   if (!acceptList.empty())
   {
      chatClientServicePtr_->AcceptNewPublicKeys(acceptList);
   }

   if (!declineList.empty())
   {
      chatClientServicePtr_->DeclineNewPublicKeys(declineList);
   }
}

void ChatWidget::onOtcRequestCurrentChanged(const QModelIndex &current, const QModelIndex &previous) const
{
   onOtcPublicUpdated();

   QDateTime selectedPeerTimeStamp;
   if (currentPartyId_ == Chat::OtcRoomName) {
      if (current.isValid() && current.row() >= 0 && current.row() < int(otcHelper_->client()->requests().size())) {
         selectedPeerTimeStamp = otcHelper_->client()->requests().at(size_t(current.row()))->request.timestamp;
      }
   }

   otcHelper_->setGlobalOTCEntryTimeStamp(selectedPeerTimeStamp);
}

void ChatWidget::onOtcPrivatePartyReady(const Chat::ClientPartyPtr& clientPartyPtr) const
{
   stateCurrent_->onOtcPrivatePartyReady(clientPartyPtr);
}

void ChatWidget::onPrivateMessagesHistoryCount(const std::string& partyId, quint64 count) const
{
   if (currentPartyId_ != partyId) {
      return;
   }

   if (ui_->textEditMessages->messagesCount(partyId) < count)
   {
      ui_->showHistoryButton->setVisible(true);
      return;
   }

   ui_->showHistoryButton->setVisible(false);
}

void ChatWidget::onRequestAllPrivateMessages() const
{
   chatClientServicePtr_->RequestAllHistoryMessages(currentPartyId_);
}
