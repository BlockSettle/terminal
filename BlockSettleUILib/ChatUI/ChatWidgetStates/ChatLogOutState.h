#ifndef CHATLOGOUTSTATE_H
#define CHATLOGOUTSTATE_H

#include "AbstractChatWidgetState.h"
#include "ChatUI/ChatOTCHelper.h"

class ChatLogOutState : public AbstractChatWidgetState {
public:
   explicit ChatLogOutState(ChatWidget* chat) : AbstractChatWidgetState(chat) { enterState(); }
   ~ChatLogOutState() override = default;
protected:
   void applyUserFrameChange() override {
      auto* searchWidget = chat_->ui_->searchWidget;
      searchWidget->onClearLineEdit();
      searchWidget->onSetLineEditEnabled(false);

      if (chat_->chatPartiesTreeModel_) {
         chat_->chatPartiesTreeModel_->onCleanModel();
      }

      chat_->ui_->labelUserName->setText(QObject::tr("offline"));
   }
   void applyChatFrameChange() override {
      chat_->ui_->textEditMessages->onLogout();

      chat_->ui_->frameContactActions->setVisible(false);

      chat_->ui_->input_textEdit->setText(QLatin1Literal(""));
      chat_->ui_->input_textEdit->setVisible(false);
      chat_->ui_->input_textEdit->setEnabled(false);

      chat_->draftMessages_.clear();
   }
   void applyRoomsFrameChange() override {
      if (chat_->otcHelper_) {
         chat_->otcHelper_->onLogout();
      }
   }

   bool canReceiveMessage() const override { return false; }
   bool canResetReadMessage() const override { return false; }
   bool canResetPartyModel() const override { return false; }
   bool canChangeMessageState() const override { return false; }
   bool canAcceptPartyRequest() const override { return false; }
   bool canRejectPartyRequest() const override { return false; }
   bool canSendPartyRequest() const override { return false; }
   bool canRemovePartyRequest() const override { return false; }
   bool canUpdatePartyName() const override { return false; }
};

#endif // CHATLOGOUTSTATE_H
