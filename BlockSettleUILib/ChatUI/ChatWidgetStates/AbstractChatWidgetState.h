#ifndef ABSTRACTCHATWIDGETSTATE_H
#define ABSTRACTCHATWIDGETSTATE_H

#include "ChatProtocol/Message.h"
#include "ChatProtocol/ClientParty.h"
#include "ui_ChatWidget.h"

enum class OTCPages : int
{
   OTCShield = 0,
   OTCCreateRequestPage,
   OTCCreateResponsePage,
   OTCPullOwnOTCRequestPage,
   OTCNegotiateRequestPage,
   OTCNegotiateResponsePage
};

namespace Blocksettle {
   namespace Communication {
      namespace ProxyTerminalPb {
         class Response;
      }
   }
}

enum class StackedMessages {
   TextEditMessage = 0,
   OTCTable = 1
};

class ChatWidget;
class AbstractChatWidgetState
{
public:
   explicit AbstractChatWidgetState(ChatWidget* chat);
   virtual ~AbstractChatWidgetState() = default;

   void applyState() {
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
   void onProcessOtcPbMessage(const Blocksettle::Communication::ProxyTerminalPb::Response &response);

   void onOtcUpdated(const bs::network::otc::Peer *peer);
   void onOtcPublicUpdated();
   void onUpdateOTCShield();
   void onOTCPeerError(const bs::network::otc::Peer *peer, bs::network::otc::PeerErrorType type, const std::string* errorMsg);

   void onOtcRequestSubmit();
   void onOtcResponseAccept();
   void onOtcResponseUpdate();

   void onOtcQuoteRequestSubmit();
   void onOtcQuoteResponseSubmit();

   void onOtcPullOrRejectCurrent();

   void onOtcPrivatePartyReady(const Chat::ClientPartyPtr& clientPartyPtr);

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
   virtual bool canReceiveOTCOperations() const { return true; }

   void saveDraftMessage();
   void restoreDraftMessage();

   void updateOtc();

   Chat::ClientPartyPtr getParty(const std::string& partyId) const;
   Chat::ClientPartyPtr getPartyByUserHash(const std::string& userHash) const;

   ChatWidget* chat_;
};

#endif // ABSTRACTCHATWIDGETSTATE_H
