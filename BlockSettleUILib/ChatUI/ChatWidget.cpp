#include "ChatWidget.h"

#include <spdlog/spdlog.h>
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
#include "MarketDataProvider.h"
#include "AssetManager.h"
#include "BaseCelerClient.h"
#include "ui_ChatWidget.h"

using namespace bs::network;

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

   otcWindowsManager_ = std::make_shared<OTCWindowsManager>();
   auto* sWidget = ui_->stackedWidgetOTC;
   for (int index = 0; index < sWidget->count(); ++index) {
      auto* widget = qobject_cast<OTCWindowsAdapterBase*>(sWidget->widget(index));
      if (widget) {
         widget->setChatOTCManager(otcWindowsManager_);
         connect(this, &ChatWidget::chatRoomChanged, widget, &OTCWindowsAdapterBase::chatRoomChanged);
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
   , const std::shared_ptr<ApplicationSettings>& appSettings
   , const Chat::ChatClientServicePtr& chatClientServicePtr
   , const std::shared_ptr<spdlog::logger>& loggerPtr
   , const std::shared_ptr<bs::sync::WalletsManager>& walletsMgr
   , const std::shared_ptr<AuthAddressManager> &authManager
   , const std::shared_ptr<ArmoryConnection>& armory
   , const std::shared_ptr<SignContainer>& signContainer
   , const std::shared_ptr<MarketDataProvider>& mdProvider
   , const std::shared_ptr<AssetManager>& assetManager
   , const std::shared_ptr<BaseCelerClient> &celerClient)
{
   loggerPtr_ = loggerPtr;
   celerClient_ = celerClient;

   bool isProd = appSettings->get<int>(ApplicationSettings::envConfiguration) == ApplicationSettings::PROD;
   auto env = isProd ? otc::Env::Prod : otc::Env::Test;

   // OTC
   otcHelper_ = new ChatOTCHelper(this);
   otcHelper_->init(env, loggerPtr, walletsMgr, armory, signContainer, authManager, appSettings);
   otcWindowsManager_->init(walletsMgr, authManager, mdProvider, assetManager, armory);

   chatClientServicePtr_ = chatClientServicePtr;

   installEventFilter(this);

   connect(chatClientServicePtr_.get(), &Chat::ChatClientService::clientLoggedOutFromServer, this, &ChatWidget::onLogout, Qt::QueuedConnection);
   connect(chatClientServicePtr_.get(), &Chat::ChatClientService::partyModelChanged, this, &ChatWidget::onPartyModelChanged, Qt::QueuedConnection);
   connect(chatClientServicePtr_.get(), &Chat::ChatClientService::searchUserReply, ui_->searchWidget, &SearchWidget::onSearchUserReply);
   connect(ui_->searchWidget, &SearchWidget::showUserRoom, this, &ChatWidget::onShowUserRoom);
   connect(ui_->searchWidget, &SearchWidget::contactFriendRequest, this, &ChatWidget::onContactFriendRequest);

   chatPartiesTreeModel_ = std::make_shared<ChatPartiesTreeModel>(chatClientServicePtr_, otcHelper_->client());

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

   ui_->widgetOTCShield->init(walletsMgr, authManager);
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

   connect(chatClientServicePtr_.get(), &Chat::ChatClientService::clientLoggedInToServer, this, &ChatWidget::onLogin, Qt::QueuedConnection);
   connect(chatClientServicePtr_.get(), &Chat::ChatClientService::clientLoggedOutFromServer, this, &ChatWidget::onLogout, Qt::QueuedConnection);
   connect(chatClientServicePtr_.get(), &Chat::ChatClientService::partyModelChanged, this, &ChatWidget::onPartyModelChanged, Qt::QueuedConnection);

   Chat::ClientPartyModelPtr clientPartyModelPtr = chatClientServicePtr_->getClientPartyModelPtr();
   connect(clientPartyModelPtr.get(), &Chat::ClientPartyModel::messageArrived, this, &ChatWidget::onSendArrived, Qt::QueuedConnection);
   connect(clientPartyModelPtr.get(), &Chat::ClientPartyModel::clientPartyStatusChanged, this, &ChatWidget::onClientPartyStatusChanged, Qt::QueuedConnection);
   connect(clientPartyModelPtr.get(), &Chat::ClientPartyModel::messageStateChanged, this, &ChatWidget::onMessageStateChanged, Qt::QueuedConnection);
   connect(clientPartyModelPtr.get(), &Chat::ClientPartyModel::userPublicKeyChanged, this, &ChatWidget::onUserPublicKeyChanged, Qt::QueuedConnection);

   // Connect all signal that influence on widget appearance 
   connect(clientPartyModelPtr.get(), &Chat::ClientPartyModel::messageArrived, this, &ChatWidget::onRegisterNewChangingRefresh, Qt::QueuedConnection);
   connect(clientPartyModelPtr.get(), &Chat::ClientPartyModel::clientPartyStatusChanged, this, &ChatWidget::onRegisterNewChangingRefresh, Qt::QueuedConnection);
   connect(clientPartyModelPtr.get(), &Chat::ClientPartyModel::messageStateChanged, this, &ChatWidget::onRegisterNewChangingRefresh, Qt::QueuedConnection);

   ui_->textEditMessages->onSetClientPartyModel(clientPartyModelPtr);

   otcRequestViewModel_ = new OTCRequestViewModel(otcHelper_->client(), this);
   ui_->treeViewOTCRequests->setModel(otcRequestViewModel_);
   connect(ui_->treeViewOTCRequests->selectionModel(), &QItemSelectionModel::currentChanged, this, &ChatWidget::onOtcRequestCurrentChanged);

   connect(otcHelper_->client(), &OtcClient::sendPbMessage, this, &ChatWidget::sendOtcPbMessage);
   connect(otcHelper_->client(), &OtcClient::sendContactMessage, this, &ChatWidget::onSendOtcMessage);
   connect(otcHelper_->client(), &OtcClient::sendPublicMessage, this, &ChatWidget::onSendOtcPublicMessage);
   connect(otcHelper_->client(), &OtcClient::peerUpdated, this, &ChatWidget::onOtcUpdated);
   connect(otcHelper_->client(), &OtcClient::publicUpdated, this, &ChatWidget::onOtcPublicUpdated);
   connect(otcHelper_->client(), &OtcClient::peerError, this, &ChatWidget::onOTCPeerError);

   connect(ui_->widgetNegotiateRequest, &OTCNegotiationRequestWidget::requestCreated, this, &ChatWidget::onOtcRequestSubmit);
   connect(ui_->widgetPullOwnOTCRequest, &PullOwnOTCRequestWidget::currentRequestPulled, this, &ChatWidget::onOtcPullOrRejectCurrent);
   connect(ui_->widgetPullOwnOTCRequest, &PullOwnOTCRequestWidget::requestPulled, this, &ChatWidget::onOtcPullOrReject);
   connect(ui_->widgetNegotiateResponse, &OTCNegotiationResponseWidget::responseAccepted, this, &ChatWidget::onOtcResponseAccept);
   connect(ui_->widgetNegotiateResponse, &OTCNegotiationResponseWidget::responseUpdated, this, &ChatWidget::onOtcResponseUpdate);
   connect(ui_->widgetNegotiateResponse, &OTCNegotiationResponseWidget::responseRejected, this, &ChatWidget::onOtcPullOrRejectCurrent);
   connect(ui_->widgetCreateOTCRequest, &CreateOTCRequestWidget::requestCreated, this, &ChatWidget::onOtcQuoteRequestSubmit);
   connect(ui_->widgetCreateOTCResponse, &CreateOTCResponseWidget::responseCreated, this, &ChatWidget::onOtcQuoteResponseSubmit);

   ui_->widgetCreateOTCRequest->init(env);
}

otc::Peer *ChatWidget::currentPeer() const
{
   ChatPartiesSortProxyModel* chartProxyModel = static_cast<ChatPartiesSortProxyModel*>(ui_->treeViewUsers->model());
   PartyTreeItem* partyTreeItem = chartProxyModel->getInternalData(ui_->treeViewUsers->currentIndex());
   if (!partyTreeItem || partyTreeItem->modelType() == UI::ElementType::Container) {
      return nullptr;
   }

   if (currentPartyId_ == Chat::OtcRoomName) {
      const auto &currentIndex = ui_->treeViewOTCRequests->currentIndex();
      if (!currentIndex.isValid() || currentIndex.row() < 0 || currentIndex.row() >= int(otcHelper_->client()->requests().size())) {
         // Show by default own request (if available)
         return otcHelper_->client()->ownRequest();
      }
      return otcHelper_->client()->requests().at(size_t(currentIndex.row()));
   }

   const auto clientPartyPtr = partyTreeItem->data().value<Chat::ClientPartyPtr>();
   if (!clientPartyPtr) {
      return nullptr;
   }

   switch (partyTreeItem->peerType) {
      case otc::PeerType::Contact:     return otcHelper_->client()->contact(clientPartyPtr->userHash());
      // #new_logic : ???
      // We should use userHash here too, but for some reasons it's not set here
      case otc::PeerType::Request:     return otcHelper_->client()->request(clientPartyPtr->id());
      case otc::PeerType::Response:    return otcHelper_->client()->response(clientPartyPtr->id());
   }

   assert(false);
   return nullptr;
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

void ChatWidget::onOtcUpdated(const otc::Peer *peer)
{
   stateCurrent_->onOtcUpdated(peer);
}

void ChatWidget::onOtcPublicUpdated()
{
   stateCurrent_->onOtcPublicUpdated();
   ui_->treeViewUsers->onExpandGlobalOTC();
}

void ChatWidget::onOTCPeerError(const bs::network::otc::Peer *peer, const std::string &errorMsg)
{
   stateCurrent_->onOTCPeerError(peer, errorMsg);
}

void ChatWidget::onUpdateOTCShield()
{
   stateCurrent_->onUpdateOTCShield();
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

void ChatWidget::onProcessOtcPbMessage(const std::string& data)
{
   stateCurrent_->onProcessOtcPbMessage(data);
}

void ChatWidget::onSendOtcMessage(const std::string& contactId, const BinaryData& data)
{
   auto clientParty = chatClientServicePtr_->getClientPartyModelPtr()->getClientPartyByUserHash(contactId);
   if (!clientParty || !clientParty->isPrivateStandard()) {
      SPDLOG_LOGGER_ERROR(loggerPtr_, "can't find valid private party to send OTC message");
      return;
   }
   stateCurrent_->onSendOtcMessage(clientParty->id(), OtcUtils::serializeMessage(data));
}

void ChatWidget::onSendOtcPublicMessage(const BinaryData &data)
{
   stateCurrent_->onSendOtcPublicMessage(OtcUtils::serializePublicMessage(data));
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
}

void ChatWidget::onClientPartyStatusChanged(const Chat::ClientPartyPtr& clientPartyPtr)
{
   stateCurrent_->onChangePartyStatus(clientPartyPtr);
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
   auto transitionChange = [this, clientPartyPtr]() {
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

   emit chatRoomChanged();
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

   ChatPartiesSortProxyModel* chatProxyModel = static_cast<ChatPartiesSortProxyModel*>(ui_->treeViewUsers->model());
   Q_ASSERT(chatProxyModel);

   QModelIndex index = chatProxyModel->getProxyIndexById(currentPartyId_);
   if (!index.isValid()) {
      onActivateGlobalPartyId();
   }

   if (ui_->treeViewUsers->selectionModel()->currentIndex() != index) {
      ui_->treeViewUsers->selectionModel()->setCurrentIndex(index, QItemSelectionModel::Select);
   }
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
   stateCurrent_->onOtcRequestSubmit();
}

void ChatWidget::onOtcResponseAccept()
{
   stateCurrent_->onOtcResponseAccept();
}

void ChatWidget::onOtcResponseUpdate()
{
   stateCurrent_->onOtcResponseUpdate();
}

void ChatWidget::onOtcQuoteRequestSubmit()
{
   stateCurrent_->onOtcQuoteRequestSubmit();
}

void ChatWidget::onOtcQuoteResponseSubmit()
{
   stateCurrent_->onOtcQuoteResponseSubmit();
}

void ChatWidget::onOtcPullOrRejectCurrent()
{
   stateCurrent_->onOtcPullOrRejectCurrent();
}

void ChatWidget::onOtcPullOrReject(const std::string& contactId, bs::network::otc::PeerType type)
{
   stateCurrent_->onOtcPullOrReject(contactId, type);
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
   QString detailsPattern = tr("Contacts Require key update: %1");

   QString  detailsString = detailsPattern.arg(userPublicKeyInfoList.size());

   BSMessageBox bsMessageBox(BSMessageBox::question, tr("Contacts Information Update"),
      tr("Do you wish to import your full Contact list?"),
      tr("Press OK to Import all Contact ID keys. Selecting Cancel will allow you to determine each contact individually."),
      detailsString);
   int ret = bsMessageBox.exec();

   onConfirmContactNewKeyData(userPublicKeyInfoList, QDialog::Accepted == ret);
}

void ChatWidget::onConfirmContactNewKeyData(const Chat::UserPublicKeyInfoList& userPublicKeyInfoList, bool bForceUpdateAllUsers)
{
   Chat::UserPublicKeyInfoList acceptList;
   Chat::UserPublicKeyInfoList declineList;

   for (const auto& userPkPtr : userPublicKeyInfoList)
   {
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

void ChatWidget::onOtcRequestCurrentChanged(const QModelIndex &current, const QModelIndex &previous)
{
   onOtcPublicUpdated();
}
