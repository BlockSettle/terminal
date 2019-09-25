#ifndef ABSTRACTCHATWIDGETSTATE_H
#define ABSTRACTCHATWIDGETSTATE_H

#include "ChatUI/ChatWidget.h"
#include "ChatProtocol/Message.h"
#include "ChatProtocol/ClientParty.h"
#include "ui_ChatWidget.h"

// #old_logic : delete this all widget and use class RFQShieldPage(maybe need redo it but based class should be the same)
enum class OTCPages : int
{
   OTCShield = 0,
   OTCCreateRequestPage,
   OTCCreateResponsePage,
   OTCPullOwnOTCRequestPage,
   OTCNegotiateRequestPage,
   OTCNegotiateResponsePage
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
   void onSendMessage();
   void onProcessMessageArrived(const Chat::MessagePtrList& messagePtr);
   void onChangePartyStatus(const Chat::ClientPartyPtr& clientPartyPtr);
   void onResetPartyModel();
   void onMessageRead(const std::string& partyId, const std::string& messageId);
   void onChangeMessageState(const std::string& partyId, const std::string& message_id, const int party_message_state);
   void onAcceptPartyRequest(const std::string& partyId);
   void onRejectPartyRequest(const std::string& partyId);
   void onSendPartyRequest(const std::string& partyId);
   void onRemovePartyRequest(const std::string& partyId);
   void onNewPartyRequest(const std::string& partyName);
   void onUpdateDisplayName(const std::string& partyId, const std::string& contactName);


   // OTC
   void onSendOtcMessage(const std::string &partyId, const std::string& data);
   void onSendOtcPublicMessage(const std::string& data);
   void onProcessOtcPbMessage(const std::string& data);

   void onOtcUpdated(const bs::network::otc::Peer *peer);
   void onOtcPublicUpdated();

   void onOtcRequestSubmit();
   void onOtcResponseAccept();
   void onOtcResponseUpdate();

   void onOtcQuoteRequestSubmit();
   void onOtcQuoteResponseSubmit();

   void onOtcPullOrRejectCurrent();

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
   virtual bool canPerformOTCOperations() const { return false; }

   void saveDraftMessage();
   void restoreDraftMessage();

   void updateOtc();

   Chat::ClientPartyPtr getParty(const std::string& partyId) const;

   ChatWidget* chat_;
};

#endif // ABSTRACTCHATWIDGETSTATE_H
