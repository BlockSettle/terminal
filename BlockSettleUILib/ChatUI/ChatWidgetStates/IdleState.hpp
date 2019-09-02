#include "AbstractChatWidgetState.h"

class IdleState : public AbstractChatWidgetState {
public:
   explicit IdleState(ChatWidget* chat) : AbstractChatWidgetState(chat) { enterState(); }
   virtual ~IdleState() override = default;
protected:
   virtual void applyUserFrameChange() override {
      chat_->ui_->searchWidget->setLineEditEnabled(true);
      chat_->ui_->textEditMessages->switchToChat({});

      chat_->ui_->labelUserName->setText(QString::fromStdString(chat_->ownUserId_));
   }
   virtual void applyChatFrameChange() override {
      chat_->ui_->textEditMessages->setOwnUserId(chat_->ownUserId_);
      chat_->ui_->textEditMessages->switchToChat(chat_->currentPartyId_);

      chat_->ui_->frameContactActions->setVisible(false);

      chat_->ui_->input_textEdit->setText({});
      chat_->ui_->input_textEdit->setVisible(true);
      chat_->ui_->input_textEdit->setEnabled(false);
   }
   virtual void applyRoomsFrameChange() override {
      chat_->ui_->stackedWidgetOTC->setCurrentIndex(static_cast<int>(OTCPages::OTCGeneralRoomShieldPage));
   }
};
