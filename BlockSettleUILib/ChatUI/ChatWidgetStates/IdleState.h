/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
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
      chat_->ui_->labelUserName->setProperty("headerLabelActivated", true);
      chat_->ui_->labelUserName->setTextInteractionFlags(Qt::TextSelectableByMouse);
      qApp->style()->unpolish(chat_->ui_->labelUserName);
      qApp->style()->polish(chat_->ui_->labelUserName);
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
