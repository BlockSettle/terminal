/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef PRIVATEPARTYREQUESTEDINCOMINGSTATE_H
#define PRIVATEPARTYREQUESTEDINCOMINGSTATE_H

#include "AbstractChatWidgetState.h"

namespace {
   // translation
   const QString buttonAcceptPartyText = QObject::tr("ACCEPT");
   const QString buttonRejectPartyText = QObject::tr("REJECT");
}


class PrivatePartyRequestedIncomingState : public AbstractChatWidgetState {
public:
   explicit PrivatePartyRequestedIncomingState(ChatWidget* chat) : AbstractChatWidgetState(chat) {}
   ~PrivatePartyRequestedIncomingState() override = default;
protected:
   void applyUserFrameChange() override {}
   void applyChatFrameChange() override {
      chat_->ui_->textEditMessages->onSwitchToChat(chat_->currentPartyId_);

      chat_->ui_->pushButton_AcceptSend->setText(buttonAcceptPartyText);
      chat_->ui_->pushButton_AcceptSend->disconnect();
      QObject::connect(chat_->ui_->pushButton_AcceptSend, &QPushButton::clicked, chat_, [this]() {
         chat_->onContactRequestAcceptClicked(chat_->currentPartyId_);
      });

      chat_->ui_->pushButton_RejectCancel->setText(buttonRejectPartyText);
      chat_->ui_->pushButton_RejectCancel->disconnect();
      QObject::connect(chat_->ui_->pushButton_RejectCancel, &QPushButton::clicked, chat_, [this]() {
         chat_->onContactRequestRejectClicked(chat_->currentPartyId_);
      });

      chat_->ui_->frameContactActions->setVisible(true);

      chat_->ui_->input_textEdit->setText({});
      chat_->ui_->input_textEdit->setVisible(true);
      chat_->ui_->input_textEdit->setEnabled(false);
   }
   void applyRoomsFrameChange() override {
      chat_->ui_->widgetOTCShield->showOtcAvailableOnlyForAcceptedContacts();
   }
};

#endif // PRIVATEPARTYREQUESTEDINCOMINGSTATE_H
