/*

***********************************************************************************
* Copyright (C) 2019 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef PRIVATEPARTYINITSTATE_H
#define PRIVATEPARTYINITSTATE_H

#include "AbstractChatWidgetState.h"
#include "ChatUI/ChatWidget.h"
#include "OtcClient.h"
#include "ChatUI/OTCShieldWidgets/OTCWindowsAdapterBase.h"
#include "ui_ChatWidget.h"

class PrivatePartyInitState : public AbstractChatWidgetState {
public:
   explicit PrivatePartyInitState(ChatWidget* chat) : AbstractChatWidgetState(chat) {}
   ~PrivatePartyInitState() override {
      saveDraftMessage();

      // leave per, so let's send ownership over reservation to otc client if any
      auto* otcWidget = qobject_cast<OTCWindowsAdapterBase*>(chat_->ui_->stackedWidgetOTC->currentWidget());
      if (otcWidget) {
         auto reservation = otcWidget->releaseReservation();
         const auto &peer = chat_->currentPeer();
         if (reservation.isValid() && peer) {
            chat_->otcHelper_->client()->setReservation(peer, std::move(reservation));
         }
      }
   };
protected:
   void applyUserFrameChange() override {}
   void applyChatFrameChange() override {
      Chat::ClientPartyPtr clientPartyPtr = getParty(chat_->currentPartyId_);

      assert(clientPartyPtr);
      if (clientPartyPtr->isGlobalOTC()) {
         chat_->ui_->treeViewOTCRequests->selectionModel()->reset();
         chat_->ui_->stackedWidgetMessages->setCurrentIndex(static_cast<int>(StackedMessages::OTCTable));
         chat_->ui_->showHistoryButton->setVisible(false);
         return;
      }

      chat_->ui_->stackedWidgetMessages->setCurrentIndex(static_cast<int>(StackedMessages::TextEditMessage));
      chat_->ui_->textEditMessages->onSwitchToChat(chat_->currentPartyId_);

      chat_->ui_->frameContactActions->setVisible(false);

      chat_->ui_->input_textEdit->setText({});
      chat_->ui_->input_textEdit->setVisible(true);
      chat_->ui_->input_textEdit->setEnabled(true);
      chat_->ui_->input_textEdit->setFocus();

      restoreDraftMessage();
   }
   void applyRoomsFrameChange() override {
      Chat::ClientPartyPtr clientPartyPtr = getParty(chat_->currentPartyId_);

      auto checkIsTradingParticipant = [&]() -> bool {
         const auto userCelerType = chat_->userType_;
         if (bs::network::UserType::Dealing != userCelerType
            && bs::network::UserType::Trading != userCelerType) {
            chat_->ui_->widgetOTCShield->showOtcAvailableToTradingParticipants();
            return false;
         }

         return true;
      };

      if (!clientPartyPtr) {
         updateOtc();
         return;
      }

      if (clientPartyPtr->isGlobalOTC()) {
         if (!checkIsTradingParticipant()) {
            return;
         }
      }
      else if (clientPartyPtr->isGlobal()) {
         chat_->ui_->widgetOTCShield->showChatExplanation();
         return;
      }
      else if (clientPartyPtr->isPrivate()) {
         if (!checkIsTradingParticipant()) {
            return;
         }

         if (clientPartyPtr->clientStatus() == Chat::ClientStatus::OFFLINE) {
            chat_->ui_->widgetOTCShield->showContactIsOffline();
            return;
         }

         Chat::PartyRecipientPtr recipientPtr = clientPartyPtr->getSecondRecipient(chat_->ownUserId_);
         CelerClient::CelerUserType counterPartyCelerType = recipientPtr->celerType();
         if (counterPartyCelerType != bs::network::UserType::Dealing
            && counterPartyCelerType != bs::network::UserType::Trading) {
            chat_->ui_->widgetOTCShield->showCounterPartyIsntTradingParticipant();
            return;
         }
         // check other party
      }

      updateOtc();
   }
   void applyPostChanged() override {
      // enter peer chat window, let's take ownership on reservation
      auto* otcWidget = qobject_cast<OTCWindowsAdapterBase*>(chat_->ui_->stackedWidgetOTC->currentWidget());
      const auto &peer = chat_->currentPeer();
      if (otcWidget && peer) {
         otcWidget->setReservation(chat_->otcHelper_->client()->releaseReservation(peer));
      }
   };

   bool canSendMessage() const override { return true; }
   bool canPerformOTCOperations() const override { return true; }
};

#endif // PRIVATEPARTYINITSTATE_H
