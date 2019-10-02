#ifndef PRIVATEPARTYINITSTATE_H
#define PRIVATEPARTYINITSTATE_H

#include "AbstractChatWidgetState.h"
#include "BaseCelerClient.h"

namespace {
   enum class StackedMessages {
      TextEditMessage = 0,
      OTCTable = 1
   };
}

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

      // #new_logic : fix me, party should exist!
      if (clientPartyPtr && clientPartyPtr->isGlobalOTC()) {
         chat_->ui_->stackedWidgetMessages->setCurrentIndex(static_cast<int>(StackedMessages::OTCTable));
         return;
      }

      chat_->ui_->stackedWidgetMessages->setCurrentIndex(static_cast<int>(StackedMessages::TextEditMessage));
      chat_->ui_->textEditMessages->onSwitchToChat(chat_->currentPartyId_);

      chat_->ui_->frameContactActions->setVisible(false);

      chat_->ui_->input_textEdit->setText({});
      chat_->ui_->input_textEdit->setVisible(true);
      // #new_logic : fix me, party should exist! set always true
      chat_->ui_->input_textEdit->setEnabled(clientPartyPtr != nullptr);
      chat_->ui_->input_textEdit->setFocus();

      restoreDraftMessage();
   }
   void applyRoomsFrameChange() override {
      Chat::ClientPartyPtr clientPartyPtr = getParty(chat_->currentPartyId_);

      auto checkIsTradingParticipant = [&]() -> bool {
         const auto userCelerType = chat_->celerClient_->celerUserType();
         if (BaseCelerClient::Dealing != userCelerType
            && BaseCelerClient::Trading != userCelerType) {
            chat_->ui_->widgetOTCShield->showOtcAvailableToTradingParticipants();
            return false;
         }

         return true;
      };

      // #new_logic : update ClientPartyModel to receive public OTC requests/responses (clientPartyPtr should be set)
      if (!clientPartyPtr) {
         updateOtc();
         return;
      }

      if (clientPartyPtr->isGlobal()) {
         if (clientPartyPtr->displayName() == Chat::GlobalRoomName) {
            chat_->ui_->widgetOTCShield->showOtcUnavailableGlobal();
            return;
         } 
         else if (clientPartyPtr->displayName() == Chat::OtcRoomName) {
            if (!checkIsTradingParticipant()) {
               return;
            }
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

         // check other party
      }

      updateOtc();
   }

   bool canSendMessage() const override { return true; }
   bool canPerformOTCOperations() const override { return true; }
};

#endif // PRIVATEPARTYINITSTATE_H
