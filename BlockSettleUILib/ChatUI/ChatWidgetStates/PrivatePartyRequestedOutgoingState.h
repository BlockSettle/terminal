#ifndef PRIVATEPARTYREQUESTEDOUTGOINGSTATE_H
#define PRIVATEPARTYREQUESTEDOUTGOINGSTATE_H

#include "AbstractChatWidgetState.h"

class PrivatePartyRequestedOutgoingState : public AbstractChatWidgetState {
public:
   explicit PrivatePartyRequestedOutgoingState(ChatWidget* chat) : AbstractChatWidgetState(chat) { enterState(); }
   ~PrivatePartyRequestedOutgoingState() override = default;
protected:
   void applyUserFrameChange() override {}
   void applyChatFrameChange() override {
      chat_->ui_->textEditMessages->onSwitchToChat(chat_->currentPartyId_);

      chat_->ui_->frameContactActions->setVisible(false);

      chat_->ui_->input_textEdit->setText({});
      chat_->ui_->input_textEdit->setVisible(true);
      chat_->ui_->input_textEdit->setEnabled(false);
   }
   void applyRoomsFrameChange() override {}
};

#endif // PRIVATEPARTYREQUESTEDOUTGOINGSTATE_H
