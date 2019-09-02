#include "AbstractChatWidgetState.h"

class ChatLogOutState : public AbstractChatWidgetState {
public:
   explicit ChatLogOutState(ChatWidget* chat) : AbstractChatWidgetState(chat) { enterState(); }
   virtual ~ChatLogOutState() = default;
protected:
   virtual void applyUserFrameChange() override {
      auto* searchWidget = chat_->ui_->searchWidget;
      searchWidget->clearLineEdit();
      searchWidget->setLineEditEnabled(false);

      chat_->chatPartiesTreeModel_->cleanModel();

      chat_->ui_->labelUserName->setText(QObject::tr("offline"));
   }
   virtual void applyChatFrameChange() override {
      chat_->ui_->textEditMessages->logout();

      chat_->ui_->frameContactActions->setVisible(false);

      chat_->ui_->input_textEdit->setText(QLatin1Literal(""));
      chat_->ui_->input_textEdit->setVisible(false);
      chat_->ui_->input_textEdit->setEnabled(false);

      chat_->draftMessages_.clear();
   }
   virtual void applyRoomsFrameChange() override {
      chat_->ui_->stackedWidgetOTC->setCurrentIndex(static_cast<int>(OTCPages::OTCLoginRequiredShieldPage));
   }

   virtual bool canReceiveMessage() const override { return false; }
   virtual bool canChangePartyStatus() const override { return false; }
   virtual bool canResetReadMessage() const override { return false; }
   virtual bool canResetPartyModel() const override { return false; }
   virtual bool canChangeMessageState() const override { return false; }
   virtual bool canAcceptPartyRequest() const override { return false; }
   virtual bool canRejectPartyRequest() const override { return false; }
   virtual bool canSendPartyRequest() const override { return false; }
   virtual bool canRemovePartyRequest() const override { return false; }
   virtual bool canUpdatePartyName() const override { return false; }
};
