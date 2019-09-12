#ifndef PRIVATEPARTYUNINITSTATE_H
#define PRIVATEPARTYUNINITSTATE_H

#include "AbstractChatWidgetState.h"

namespace {
   // translation
   const QString buttonSentPartyText = QObject::tr("SEND");
   const QString buttonCancelPartyText = QObject::tr("CANCEL");
}


class PrivatePartyUninitState : public AbstractChatWidgetState {
public:
   explicit PrivatePartyUninitState(ChatWidget* chat) : AbstractChatWidgetState(chat) { enterState(); }
   ~PrivatePartyUninitState() override = default;
protected:
   void applyUserFrameChange() override {}
   void applyChatFrameChange() override {
      chat_->ui_->textEditMessages->onSwitchToChat(chat_->currentPartyId_);

      chat_->ui_->pushButton_AcceptSend->setText(buttonSentPartyText);
      chat_->ui_->pushButton_AcceptSend->disconnect();
      QObject::connect(chat_->ui_->pushButton_AcceptSend, &QPushButton::clicked, chat_, [this]() {
         chat_->onContactRequestSendClicked(chat_->currentPartyId_);
      });

      chat_->ui_->pushButton_RejectCancel->setText(buttonCancelPartyText);
      chat_->ui_->pushButton_RejectCancel->disconnect();
      QObject::connect(chat_->ui_->pushButton_RejectCancel, &QPushButton::clicked, chat_, [this]() {
         chat_->onContactRequestCancelClicked(chat_->currentPartyId_);
      });

      chat_->ui_->frameContactActions->setVisible(true);

      chat_->ui_->input_textEdit->setText({});
      chat_->ui_->input_textEdit->setVisible(true);
      chat_->ui_->input_textEdit->setEnabled(false);
   }
   void applyRoomsFrameChange() override {}
};

#endif // PRIVATEPARTYUNINITSTATE_H
