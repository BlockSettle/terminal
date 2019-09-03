#include "AbstractChatWidgetState.h"

class PrivatePartyRejectedState : public AbstractChatWidgetState {
public:
   explicit PrivatePartyRejectedState(ChatWidget* chat) : AbstractChatWidgetState(chat) { enterState(); }
   virtual ~PrivatePartyRejectedState() override = default;
protected:
   virtual void applyUserFrameChange() override {}
   virtual void applyChatFrameChange() override {
      chat_->ui_->textEditMessages->switchToChat(chat_->currentPartyId_);

      chat_->ui_->frameContactActions->setVisible(false);

      chat_->ui_->input_textEdit->setText({});
      chat_->ui_->input_textEdit->setVisible(true);
      chat_->ui_->input_textEdit->setEnabled(false);
   }
   virtual void applyRoomsFrameChange() override {
      // #new_logic : OTC shield?
   }
};
