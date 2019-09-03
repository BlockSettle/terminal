#include "AbstractChatWidgetState.h"

class PrivatePartyInitState : public AbstractChatWidgetState {
public:
   explicit PrivatePartyInitState(ChatWidget* chat) : AbstractChatWidgetState(chat) { enterState(); }
   virtual ~PrivatePartyInitState() override {
      saveDraftMessage();
   };
protected:
   virtual void applyUserFrameChange() override {}
   virtual void applyChatFrameChange() override {
      chat_->ui_->textEditMessages->switchToChat(chat_->currentPartyId_);

      chat_->ui_->frameContactActions->setVisible(false);

      chat_->ui_->input_textEdit->setText({});
      chat_->ui_->input_textEdit->setVisible(true);
      chat_->ui_->input_textEdit->setEnabled(true);
      chat_->ui_->input_textEdit->setFocus();

      restoreDraftMessage();
   }
   virtual void applyRoomsFrameChange() override {
      // #new_logic : OTC shield
   }
   virtual bool canSendMessage() const override { return true; }
};
