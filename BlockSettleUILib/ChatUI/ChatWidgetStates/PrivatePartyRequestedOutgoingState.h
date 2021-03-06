/*

***********************************************************************************
* Copyright (C) 2019 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef PRIVATEPARTYREQUESTEDOUTGOINGSTATE_H
#define PRIVATEPARTYREQUESTEDOUTGOINGSTATE_H

#include "AbstractChatWidgetState.h"

class PrivatePartyRequestedOutgoingState : public AbstractChatWidgetState {
public:
   explicit PrivatePartyRequestedOutgoingState(ChatWidget* chat) : AbstractChatWidgetState(chat) {}
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
   void applyRoomsFrameChange() override {
      chat_->ui_->widgetOTCShield->showShieldOtcAvailableOnceAccepted();
   }
   void applyPostChanged() override {};
};

#endif // PRIVATEPARTYREQUESTEDOUTGOINGSTATE_H
