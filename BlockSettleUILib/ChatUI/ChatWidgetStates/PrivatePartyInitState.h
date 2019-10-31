#ifndef PRIVATEPARTYINITSTATE_H
#define PRIVATEPARTYINITSTATE_H

#include "AbstractChatWidgetState.h"
#include "ChatUI/ChatWidget.h"

class PrivatePartyInitState : public AbstractChatWidgetState {
public:
   explicit PrivatePartyInitState(ChatWidget* chat) : AbstractChatWidgetState(chat) {}
   ~PrivatePartyInitState() override {
      saveDraftMessage();
   };
protected:
   void applyUserFrameChange() override {}
   void applyChatFrameChange() override {
      Chat::ClientPartyPtr clientPartyPtr = getParty(chat_->currentPartyId_);

      assert(clientPartyPtr);
      if (clientPartyPtr->isGlobalOTC()) {
         chat_->ui_->treeViewOTCRequests->selectionModel()->reset();
         chat_->ui_->stackedWidgetMessages->setCurrentIndex(static_cast<int>(StackedMessages::OTCTable));
         return;
      }

      chat_->ui_->stackedWidgetMessages->setCurrentIndex(static_cast<int>(StackedMessages::TextEditMessage));
      chat_->ui_->textEditMessages->onSwitchToChat(chat_->currentPartyId_);

      chat_->ui_->frameContactActions->setVisible(false);

      chat_->ui_->input_textEdit->setText({});
      chat_->ui_->input_textEdit->setVisible(true);
      chat_->ui_->input_textEdit->setEnabled(true);
      chat_->ui_->input_textEdit->setFocus();

      restoreDraftMessage();
   }
   void applyRoomsFrameChange() override {
      Chat::ClientPartyPtr clientPartyPtr = getParty(chat_->currentPartyId_);

      auto checkIsTradingParticipant = [&]() -> bool {
         const auto userCelerType = chat_->userType_;
         if (bs::network::UserType::Dealing != userCelerType
            && bs::network::UserType::Trading != userCelerType) {
            chat_->ui_->widgetOTCShield->showOtcAvailableToTradingParticipants();
            return false;
         }

         return true;
      };

      if (!clientPartyPtr) {
         updateOtc();
         return;
      }

      if (clientPartyPtr->isGlobalOTC()) {
         if (!checkIsTradingParticipant()) {
            return;
         }
      }
      else if (clientPartyPtr->isGlobal()) {
         if (clientPartyPtr->displayName() == Chat::GlobalRoomName) {
            chat_->ui_->widgetOTCShield->showOtcUnavailableGlobal();
            return;
         } 
         else if (clientPartyPtr->displayName() == Chat::SupportRoomName) {
            chat_->ui_->widgetOTCShield->showOtcUnavailableSupport();
            return;
         }
      }
      else if (clientPartyPtr->isPrivate()) {
         if (!checkIsTradingParticipant()) {
            return;
         }

         if (clientPartyPtr->clientStatus() == Chat::ClientStatus::OFFLINE) {
            chat_->ui_->widgetOTCShield->showContactIsOffline();
            return;
         }

         Chat::PartyRecipientPtr recipientPtr = clientPartyPtr->getSecondRecipient(chat_->ownUserId_);
         CelerClient::CelerUserType counterPartyCelerType = recipientPtr->celerType();
         if (counterPartyCelerType != bs::network::UserType::Dealing
            && counterPartyCelerType != bs::network::UserType::Trading) {
            chat_->ui_->widgetOTCShield->showCounterPartyIsntTradingParticipant();
            return;
         }
         // check other party
      }

      updateOtc();
   }

   bool canSendMessage() const override { return true; }
   bool canPerformOTCOperations() const override { return true; }
};

#endif // PRIVATEPARTYINITSTATE_H
