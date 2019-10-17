#include "AbstractChatWidgetState.h"

#include "ChatUI/ChatWidget.h"
#include "ChatUI/ChatOTCHelper.h"
#include "ChatUI/ChatPartiesTreeModel.h"
#include "ChatUI/OTCRequestViewModel.h"
#include "NotificationCenter.h"
#include "OtcClient.h"
#include "OtcUtils.h"

namespace {
   const int kMaxMessageNotifLength = 20;
   const auto kTimeoutErrorWith = QObject::tr("OTC negotiation with %1 timed out");
   const auto kCancelledErrorWith = QObject::tr("OTC negotiation with %1 was canceled");
   const auto kRejectedErrorWith = QObject::tr("OTC order was rejected: %1");
} // namespace

using namespace bs::network;

AbstractChatWidgetState::AbstractChatWidgetState(ChatWidget* chat)
   : chat_(chat)
{
}

void AbstractChatWidgetState::onSendMessage()
{
   if (!canSendMessage()) {
      return;
   }

   std::string messageText = chat_->ui_->input_textEdit->toPlainText().toStdString();
   if (messageText.empty()) {
      return;
   }

   chat_->chatClientServicePtr_->SendPartyMessage(chat_->currentPartyId_, messageText);
   chat_->ui_->input_textEdit->clear();
}

void AbstractChatWidgetState::onProcessMessageArrived(const Chat::MessagePtrList& messagePtr)
{
   if (!canReceiveMessage()) {
      return;
   }

   if (messagePtr.empty()) {
      return;
   }

   // Update all UI elements
   int bNewMessagesCounter = 0;
   const std::string& partyId = messagePtr[0]->partyId();

   Chat::ClientPartyPtr clientPartyPtr = getParty(partyId);

   // Tab notifier
   for (int iMessage = 0; iMessage < messagePtr.size(); ++iMessage) {
      Chat::MessagePtr message = messagePtr[iMessage];
      if (static_cast<Chat::PartyMessageState>(message->partyMessageState()) == Chat::PartyMessageState::SENT &&
         chat_->ownUserId_ != message->senderHash()) {
         ++bNewMessagesCounter;

         const auto messageTitle = clientPartyPtr->displayName();
         auto messageText = message->messageText();
         const auto otcText = OtcUtils::toReadableString(QString::fromStdString(messageText));

         if (otcText.isEmpty() && messageText.length() > kMaxMessageNotifLength) {
            messageText = messageText.substr(0, kMaxMessageNotifLength) + "...";
         }

         bs::ui::NotifyMessage notifyMsg;
         notifyMsg.append(QString::fromStdString(messageTitle));
         notifyMsg.append(otcText.isEmpty() ? QString::fromStdString(messageText) : otcText);
         notifyMsg.append(QString::fromStdString(partyId));

         NotificationCenter::notify(bs::ui::NotifyType::UpdateUnreadMessage, notifyMsg);
      }
   }

   // Update tree
   if (bNewMessagesCounter > 0 && partyId != chat_->currentPartyId_) {
      chat_->chatPartiesTreeModel_->onIncreaseUnseenCounter(partyId, bNewMessagesCounter);
   }

   chat_->ui_->textEditMessages->onMessageUpdate(messagePtr);

   if (canPerformOTCOperations()) {
      chat_->otcHelper_->onMessageArrived(messagePtr);
   }
}

void AbstractChatWidgetState::onChangePartyStatus(const Chat::ClientPartyPtr& clientPartyPtr)
{
   if (!canChangePartyStatus()) {
      return;
   }

   chat_->chatPartiesTreeModel_->onPartyStatusChanged(clientPartyPtr);
   chat_->otcHelper_->onPartyStateChanged(clientPartyPtr);
   onUpdateOTCShield();
}

void AbstractChatWidgetState::onResetPartyModel()
{
   if (!canResetPartyModel()) {
      return;
   }

   chat_->chatPartiesTreeModel_->onPartyModelChanged();
   chat_->onActivatePartyId(QString::fromStdString(chat_->currentPartyId_));
}

void AbstractChatWidgetState::onMessageRead(const std::string& partyId, const std::string& messageId)
{
   if (!canResetReadMessage()) {
      return;
   }

   chat_->chatClientServicePtr_->SetMessageSeen(partyId, messageId);
}

void AbstractChatWidgetState::onChangeMessageState(const std::string& partyId, const std::string& message_id, const int party_message_state)
{
   if (!canChangeMessageState()) {
      return;
   }

   const Chat::MessagePtr message = chat_->ui_->textEditMessages->onMessageStatusChanged(partyId, message_id, party_message_state);

   // Update tree view if needed
   if (static_cast<Chat::PartyMessageState>(party_message_state) == Chat::PartyMessageState::SEEN
      && message && message->senderHash() != chat_->ownUserId_) {
      chat_->chatPartiesTreeModel_->onDecreaseUnseenCounter(partyId, 1);
   }
}

void AbstractChatWidgetState::onAcceptPartyRequest(const std::string& partyId)
{
   if (!canAcceptPartyRequest()) {
      return;
   }

   chat_->chatClientServicePtr_->AcceptPrivateParty(partyId);
}

void AbstractChatWidgetState::onRejectPartyRequest(const std::string& partyId)
{
   if (!canRejectPartyRequest()) {
      return;
   }

   chat_->chatClientServicePtr_->RejectPrivateParty(partyId);
}

void AbstractChatWidgetState::onSendPartyRequest(const std::string& partyId)
{
   if (!canSendPartyRequest()) {
      return;
   }

   chat_->chatClientServicePtr_->RequestPrivateParty(partyId);
}

void AbstractChatWidgetState::onRemovePartyRequest(const std::string& partyId)
{
   if (!canRemovePartyRequest()) {
      return;
   }

   chat_->chatClientServicePtr_->DeletePrivateParty(partyId);
}

void AbstractChatWidgetState::onNewPartyRequest(const std::string& partyName, const std::string& initialMessage)
{
   if (!canSendPartyRequest()) {
      return;
   }

   chat_->chatClientServicePtr_->RequestPrivateParty(partyName, initialMessage);
}

void AbstractChatWidgetState::onUpdateDisplayName(const std::string& partyId, const std::string& contactName)
{
   if (!canUpdatePartyName()) {
      return;
   }

   Chat::ClientPartyModelPtr clientPartyModelPtr = chat_->chatClientServicePtr_->getClientPartyModelPtr();
   Chat::ClientPartyPtr clientPartyPtr = clientPartyModelPtr->getClientPartyById(partyId);
   clientPartyPtr->setDisplayName(contactName);

   if (clientPartyPtr->isGlobalStandard() || (clientPartyPtr->isPrivateStandard() && chat_->currentPartyId_ == partyId)) {
      chat_->ui_->textEditMessages->onUpdatePartyName(partyId);
   }
}

void AbstractChatWidgetState::onSendOtcMessage(const std::string &partyId, const std::string& data)
{
   if (canPerformOTCOperations()) {
      chat_->chatClientServicePtr_->SendPartyMessage(partyId, data);
   }
}

void AbstractChatWidgetState::onSendOtcPublicMessage(const std::string &data)
{
   if (canPerformOTCOperations()) {
      chat_->chatClientServicePtr_->SendPartyMessage(Chat::OtcRoomName, data);
   }
}

void AbstractChatWidgetState::onProcessOtcPbMessage(const Blocksettle::Communication::ProxyTerminalPb::Response &response)
{
   if (canReceiveOTCOperations()) {
      chat_->otcHelper_->onProcessOtcPbMessage(response);
   }
}

void AbstractChatWidgetState::onOtcUpdated(const otc::Peer *peer)
{
   if (canReceiveOTCOperations() && chat_->currentPeer() == peer) {
      onUpdateOTCShield();
   }
}

void AbstractChatWidgetState::onOtcPublicUpdated()
{
   if (!canReceiveOTCOperations()) {
      return;
   }

   onUpdateOTCShield();
   chat_->chatPartiesTreeModel_->onGlobalOTCChanged();
}

void AbstractChatWidgetState::onUpdateOTCShield()
{
   if (!canReceiveOTCOperations()) {
      return;
   }

   applyRoomsFrameChange();
}

void AbstractChatWidgetState::onOTCPeerError(const bs::network::otc::Peer *peer, bs::network::otc::PeerErrorType type, const std::string* errorMsg)
{
   if (!canReceiveOTCOperations()) {
      return;
   }
      
   const Chat::ClientPartyPtr clientPartyPtr = getPartyByUserHash(peer->contactId);
   if (!clientPartyPtr) {
      return;
   }  

   QString MessageBody;
   switch (type)
   {
   case bs::network::otc::PeerErrorType::Timeout:
      MessageBody = kTimeoutErrorWith.arg(QString::fromStdString(clientPartyPtr->displayName()));
      break;
   case bs::network::otc::PeerErrorType::Canceled:
      MessageBody = kCancelledErrorWith.arg(QString::fromStdString(clientPartyPtr->displayName()));
      break;
   case bs::network::otc::PeerErrorType::Rejected:
      assert(errorMsg);
      MessageBody = kRejectedErrorWith.arg(QString::fromStdString(*errorMsg));
      break;
   default:
      break;
   }
   bs::ui::NotifyMessage notifyMsg;
   notifyMsg.append(MessageBody);

   NotificationCenter::notify(bs::ui::NotifyType::OTCOrderError, notifyMsg);
}

void AbstractChatWidgetState::onOtcRequestSubmit()
{
   if (canPerformOTCOperations()) {
      chat_->otcHelper_->onOtcRequestSubmit(chat_->currentPeer(), chat_->ui_->widgetNegotiateRequest->offer());
   }
}

void AbstractChatWidgetState::onOtcResponseAccept()
{
   if (canPerformOTCOperations()) {
      chat_->otcHelper_->onOtcResponseAccept(chat_->currentPeer(), chat_->ui_->widgetNegotiateResponse->offer());
   }
}

void AbstractChatWidgetState::onOtcResponseUpdate()
{
   if (canPerformOTCOperations()) {
      chat_->otcHelper_->onOtcResponseUpdate(chat_->currentPeer(), chat_->ui_->widgetNegotiateResponse->offer());
   }
}

void AbstractChatWidgetState::onOtcQuoteRequestSubmit()
{
   if (canPerformOTCOperations()) {
      chat_->otcHelper_->onOtcQuoteRequestSubmit(chat_->ui_->widgetCreateOTCRequest->request());
   }
}

void AbstractChatWidgetState::onOtcQuoteResponseSubmit()
{
   if (canPerformOTCOperations()) {
      chat_->chatClientServicePtr_->RequestPrivatePartyOTC(chat_->currentPeer()->contactId);
   }
}

void AbstractChatWidgetState::onOtcPrivatePartyReady(const Chat::ClientPartyPtr& clientPartyPtr)
{
   if (canPerformOTCOperations() && clientPartyPtr->isPrivateOTC()) {
      Chat::PartyRecipientsPtrList recipients = clientPartyPtr->getRecipientsExceptMe(chat_->ownUserId_);
      for (const auto& recipient : recipients) {
         if (chat_->currentPeer() && recipient->userHash() == chat_->currentPeer()->contactId) {
            // found user, send request
            chat_->otcHelper_->onOtcQuoteResponseSubmit(chat_->currentPeer(), chat_->ui_->widgetCreateOTCResponse->response());
         }
      }
   }
}

void AbstractChatWidgetState::onOtcPullOrRejectCurrent()
{
   if (canPerformOTCOperations()) {
      auto peer = chat_->currentPeer();
      if (!peer) {
         assert(false);
         return;
      }

      Chat::ClientPartyModelPtr clientPartyModelPtr = chat_->chatClientServicePtr_->getClientPartyModelPtr();
      Chat::ClientPartyPtr clientPartyPtr = clientPartyModelPtr->getOtcPartyForUsers(chat_->ownUserId_, peer->contactId);
      if (clientPartyPtr) {
         chat_->chatClientServicePtr_->DeletePrivateParty(clientPartyPtr->id());
      }

      chat_->ui_->treeViewOTCRequests->selectionModel()->clearCurrentIndex();
      chat_->otcHelper_->onOtcPullOrReject(peer);
   }
}

void AbstractChatWidgetState::saveDraftMessage()
{
   const auto draft = chat_->ui_->input_textEdit->toPlainText();

   if (draft.isEmpty()) {
      chat_->draftMessages_.remove(chat_->currentPartyId_);
   }
   else {
      chat_->draftMessages_.insert(chat_->currentPartyId_, draft);
   }
}

void AbstractChatWidgetState::restoreDraftMessage()
{
   const auto iDraft = chat_->draftMessages_.find(chat_->currentPartyId_);
   if (iDraft != chat_->draftMessages_.cend()) {
      chat_->ui_->input_textEdit->setText(iDraft.value());
      auto cursor = chat_->ui_->input_textEdit->textCursor();
      cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::MoveAnchor);
      chat_->ui_->input_textEdit->setTextCursor(cursor);
   }
}

void AbstractChatWidgetState::updateOtc()
{
   if (!canPerformOTCOperations()) {
      chat_->ui_->widgetOTCShield->showOtcAvailableToTradingParticipants();
      return;
   }

   const bool globalRoom = chat_->currentPartyId_ == Chat::OtcRoomName;
   const bs::network::otc::Peer* peer = chat_->currentPeer();
   if (!peer && !globalRoom) {
      chat_->ui_->widgetOTCShield->showContactIsOffline();
      return;
   }

   if (chat_->ui_->widgetOTCShield->onRequestCheckWalletSettings()) {
      return;
   }

   if (!peer) {
      // Must be in globalRoom if checks above hold
      assert(globalRoom);
      chat_->ui_->stackedWidgetOTC->setCurrentIndex(static_cast<int>(OTCPages::OTCCreateRequestPage));
      return;
   }

   using bs::network::otc::State;
   OTCPages pageNumber = OTCPages::OTCShield;
   switch (peer->state) {
      case State::Idle:
         if (peer->type == otc::PeerType::Contact) {
            pageNumber = OTCPages::OTCNegotiateRequestPage;
         } else if (peer->isOwnRequest) {
            chat_->ui_->widgetPullOwnOTCRequest->setRequest(peer->request);
            pageNumber = OTCPages::OTCPullOwnOTCRequestPage;
         } else if (peer->type == otc::PeerType::Request) {
            chat_->ui_->widgetCreateOTCResponse->setRequest(peer->request);
            pageNumber = OTCPages::OTCCreateResponsePage;
         }
         break;
      case State::QuoteSent:
         chat_->ui_->widgetPullOwnOTCRequest->setResponse(peer->response);
         pageNumber = OTCPages::OTCPullOwnOTCRequestPage;
         break;
      case State::QuoteRecv:
         pageNumber = OTCPages::OTCNegotiateRequestPage;
         break;
      case State::OfferSent:
         chat_->ui_->widgetPullOwnOTCRequest->setOffer(peer->offer);
         pageNumber = OTCPages::OTCPullOwnOTCRequestPage;
         break;
      case State::OfferRecv:
         chat_->ui_->widgetNegotiateResponse->setOffer(peer->offer);
         pageNumber = OTCPages::OTCNegotiateResponsePage;
         break;
      case State::SentPayinInfo:
      case State::WaitPayinInfo:
         chat_->ui_->widgetOTCShield->showOtcSetupTransaction();
         return;
      case State::WaitBuyerSign:
         chat_->ui_->widgetPullOwnOTCRequest->setPendingBuyerSign(peer->offer);
         pageNumber = OTCPages::OTCPullOwnOTCRequestPage;
         break;
      case State::WaitSellerSeal:
         chat_->ui_->widgetPullOwnOTCRequest->setPendingSellerSign(peer->offer);
         pageNumber = OTCPages::OTCPullOwnOTCRequestPage;
         break;
      case State::WaitVerification:
      case State::WaitSellerSign:
         chat_->ui_->widgetOTCShield->showOtcSetupTransaction();
         return;
      case State::Blacklisted:
         chat_->ui_->widgetOTCShield->showContactIsOffline();
         return;
      default:
         assert(false && " Did you forget to handle new otc::State state? ");
         break;
   }

   auto* actionWidget = qobject_cast<OTCWindowsAdapterBase*>(chat_->ui_->stackedWidgetOTC->widget(static_cast<int>(pageNumber)));
   if (actionWidget) {
      actionWidget->setPeer(*peer);
      actionWidget->onAboutToApply();
   }

   chat_->ui_->stackedWidgetOTC->setCurrentIndex(static_cast<int>(pageNumber));
}

Chat::ClientPartyPtr AbstractChatWidgetState::getParty(const std::string& partyId) const
{
   Chat::ClientPartyModelPtr partyModel = chat_->chatClientServicePtr_->getClientPartyModelPtr();
   return partyModel->getClientPartyById(partyId);
}

Chat::ClientPartyPtr AbstractChatWidgetState::getPartyByUserHash(const std::string& userHash) const
{
   Chat::ClientPartyModelPtr partyModel = chat_->chatClientServicePtr_->getClientPartyModelPtr();
   return partyModel->getStandardPartyForUsers(chat_->ownUserId_, userHash);
}
