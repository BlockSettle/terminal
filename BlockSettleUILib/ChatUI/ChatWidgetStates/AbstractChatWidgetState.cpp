#include "AbstractChatWidgetState.h"
#include "NotificationCenter.h"
#include "ChatUI/ChatPartiesTreeModel.h"

namespace {
   const int kMaxMessageNotifLength = 20;
}

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

   Chat::ClientPartyModelPtr partyModel = chat_->chatClientServicePtr_->getClientPartyModelPtr();
   Chat::ClientPartyPtr clientPartyPtr = partyModel->getClientPartyById(partyId);

   // Tab notifier
   for (int iMessage = 0; iMessage < messagePtr.size(); ++iMessage) {
      Chat::MessagePtr message = messagePtr[iMessage];
      if (static_cast<Chat::PartyMessageState>(message->partyMessageState()) == Chat::PartyMessageState::SENT &&
         chat_->ownUserId_ != message->senderHash()) {
         ++bNewMessagesCounter;

         auto messageTitle = clientPartyPtr->displayName();
         auto messageText = message->messageText();

         if (messageText.length() > kMaxMessageNotifLength) {
            messageText = messageText.substr(0, kMaxMessageNotifLength) + "...";
         }

         bs::ui::NotifyMessage notifyMsg;
         notifyMsg.append(QString::fromStdString(messageTitle));
         notifyMsg.append(QString::fromStdString(messageText));
         notifyMsg.append(QString::fromStdString(partyId));

         NotificationCenter::notify(bs::ui::NotifyType::UpdateUnreadMessage, notifyMsg);
      }
   }

   // Update tree
   if (bNewMessagesCounter > 0 && partyId != chat_->currentPartyId_) {
      chat_->chatPartiesTreeModel_->onIncreaseUnseenCounter(partyId, bNewMessagesCounter);
   }

   chat_->ui_->textEditMessages->onMessageUpdate(messagePtr);
}

void AbstractChatWidgetState::onChangePartyStatus(const Chat::ClientPartyPtr& clientPartyPtr)
{
   if (!canChangePartyStatus()) {
      return;
   }

   chat_->chatPartiesTreeModel_->onPartyStatusChanged(clientPartyPtr);
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
      && message->senderHash() != chat_->ownUserId_) {
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

void AbstractChatWidgetState::onNewPartyRequest(const std::string& partyName)
{
   if (!canSendPartyRequest()) {
      return;
   }

   chat_->chatClientServicePtr_->RequestPrivateParty(partyName);
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
