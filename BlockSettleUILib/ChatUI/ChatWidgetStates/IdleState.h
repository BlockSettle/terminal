#ifndef IDLESTATE_H
#define IDLESTATE_H

#include "AbstractChatWidgetState.h"

class IdleState : public AbstractChatWidgetState {
public:
   explicit IdleState(ChatWidget* chat) : AbstractChatWidgetState(chat) {}
   ~IdleState() override = default;
protected:
   void applyUserFrameChange() override {
      chat_->ui_->searchWidget->onSetLineEditEnabled(true);
      chat_->ui_->textEditMessages->onSwitchToChat({});

      chat_->ui_->labelUserName->setText(QString::fromStdString(chat_->ownUserId_));
   }
   void applyChatFrameChange() override {
      chat_->ui_->textEditMessages->onSetOwnUserId(chat_->ownUserId_);
      chat_->ui_->textEditMessages->onSwitchToChat(chat_->currentPartyId_);

      chat_->ui_->frameContactActions->setVisible(false);

      chat_->ui_->input_textEdit->setText({});
      chat_->ui_->input_textEdit->setVisible(true);
      chat_->ui_->input_textEdit->setEnabled(false);
   }
   void applyRoomsFrameChange() override {}
};

#endif // IDLESTATE_H
