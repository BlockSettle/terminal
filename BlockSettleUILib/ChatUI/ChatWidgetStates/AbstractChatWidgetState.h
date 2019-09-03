#ifndef AbstractChatWidgetState_H
#define AbstractChatWidgetState_H

#include "ChatUI/ChatWidget.h"
#include "ChatProtocol/Message.h"
#include "ChatProtocol/ClientParty.h"
#include "ui_ChatWidget.h"

// #old_logic : delete this all widget and use class RFQShieldPage(maybe need redo it but based class should be the same)
enum class OTCPages : int
{
   OTCLoginRequiredShieldPage = 0,
   OTCGeneralRoomShieldPage,
   OTCCreateRequestPage,
   OTCPullOwnOTCRequestPage,
   OTCCreateResponsePage,
   OTCNegotiateRequestPage,
   OTCNegotiateResponsePage,
   OTCParticipantShieldPage,
   OTCContactShieldPage,
   OTCContactNetStatusShieldPage,
   OTCSupportRoomShieldPage
};

class AbstractChatWidgetState {
public:
   explicit AbstractChatWidgetState(ChatWidget* chat);
   virtual ~AbstractChatWidgetState() = default;

   void enterState() {
      applyUserFrameChange();
      applyChatFrameChange();
      applyRoomsFrameChange();
   }

protected:
   virtual void applyUserFrameChange() = 0;
   virtual void applyChatFrameChange() = 0;
   virtual void applyRoomsFrameChange() = 0;

   // slots
public:
   void sendMessage();
   void processMessageArrived(const Chat::MessagePtrList& messagePtr);
   void changePartyStatus(const Chat::ClientPartyPtr& clientPartyPtr);
   void resetPartyModel();
   void messageRead(const std::string& partyId, const std::string& messageId);
   void changeMessageState(const std::string& partyId, const std::string& message_id, const int party_message_state);
   void acceptPartyRequest(const std::string& partyId);
   void rejectPartyRequest(const std::string& partyId);
   void sendPartyRequest(const std::string& partyId);
   void removePartyRequest(const std::string& partyId);
   void newPartyRequest(const std::string& partyName);
   void updateDisplayName(const std::string& partyId, const std::string& contactName);

protected:

   virtual bool canSendMessage() const { return false; }
   virtual bool canReceiveMessage() const { return true; }
   virtual bool canChangePartyStatus() const { return true; }
   virtual bool canResetPartyModel() const { return true; }
   virtual bool canResetReadMessage() const { return true; }
   virtual bool canChangeMessageState() const { return true; }
   virtual bool canAcceptPartyRequest() const { return true; }
   virtual bool canRejectPartyRequest() const { return true; }
   virtual bool canSendPartyRequest() const { return true; }
   virtual bool canRemovePartyRequest() const { return true; }
   virtual bool canUpdatePartyName() const { return true; }

   void saveDraftMessage();
   void restoreDraftMessage();

   ChatWidget* chat_;
};

#endif // AbstractChatWidgetState_H
