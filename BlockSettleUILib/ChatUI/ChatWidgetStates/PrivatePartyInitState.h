#ifndef PRIVATEPARTYINITSTATE_H
#define PRIVATEPARTYINITSTATE_H

#include "AbstractChatWidgetState.h"

namespace {
   enum class StackedMessages {
      TextEditMessage = 0,
      OTCTable = 1
   };
}

class PrivatePartyInitState : public AbstractChatWidgetState {
public:
   explicit PrivatePartyInitState(ChatWidget* chat) : AbstractChatWidgetState(chat) { enterState(); }
   ~PrivatePartyInitState() override {
      saveDraftMessage();
   };
protected:
   void applyUserFrameChange() override {}
   void applyChatFrameChange() override {
      Chat::ClientPartyPtr clientPartyPtr = getParty(chat_->currentPartyId_);

      if (clientPartyPtr->isGlobalOTC()) {
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
   void applyRoomsFrameChange() override {}

   bool canSendMessage() const override { return true; }
   bool canPerformOTCOperations() const override { return true; }
};

#endif // PRIVATEPARTYINITSTATE_H
