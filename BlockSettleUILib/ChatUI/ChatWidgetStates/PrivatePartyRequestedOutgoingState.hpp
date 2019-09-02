#include "AbstractChatWidgetState.h"

class PrivatePartyRequestedOutgoingState : public AbstractChatWidgetState {
public:
   explicit PrivatePartyRequestedOutgoingState(ChatWidget* chat) : AbstractChatWidgetState(chat) { enterState(); }
   virtual ~PrivatePartyRequestedOutgoingState() override = default;
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
      // #new_logic : OTCShield? 
      // chat_->ui_->stackedWidgetOTC->setCurrentIndex(static_cast<int>(OTCPages::OTCGeneralRoomShieldPage));
   }
};