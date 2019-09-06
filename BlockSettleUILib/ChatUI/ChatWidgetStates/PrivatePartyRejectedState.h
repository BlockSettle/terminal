#ifndef PRIVATEPARTYREJECTEDSTATE_H
#define PRIVATEPARTYREJECTEDSTATE_H

#include "AbstractChatWidgetState.h"

class PrivatePartyRejectedState : public AbstractChatWidgetState {
public:
   explicit PrivatePartyRejectedState(ChatWidget* chat) : AbstractChatWidgetState(chat) { enterState(); }
   ~PrivatePartyRejectedState() override = default;
protected:
   void applyUserFrameChange() override {}
   void applyChatFrameChange() override {
      chat_->ui_->textEditMessages->onSwitchToChat(chat_->currentPartyId_);

      chat_->ui_->frameContactActions->setVisible(false);

      chat_->ui_->input_textEdit->setText({});
      chat_->ui_->input_textEdit->setVisible(true);
      chat_->ui_->input_textEdit->setEnabled(false);
   }
   void applyRoomsFrameChange() override {
      // #new_logic : OTC shield?
   }
};

#endif // PRIVATEPARTYREJECTEDSTATE_H
