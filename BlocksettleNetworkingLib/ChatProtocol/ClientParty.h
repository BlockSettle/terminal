#ifndef ClientParty_h__
#define ClientParty_h__

#include <QObject>
#include <memory>

#include "ChatProtocol/PrivateDirectMessageParty.h"

#include "chat.pb.h"

namespace Chat
{
   
   class ClientParty : public QObject, public PrivateDirectMessageParty
   {
      Q_OBJECT
   public:
      ClientParty(
         const std::string& id, 
         const PartyType& partyType, 
         const PartySubType& partySubType = PartySubType::STANDARD, 
         const PartyState& partyState = PartyState::UNINITIALIZED, 
         QObject* parent = nullptr
      );

      std::string displayName() const { return displayName_; }
      void setDisplayName(std::string val) { displayName_ = val; }

      ClientStatus clientStatus() const { return clientStatus_; }
      void setClientStatus(ClientStatus val);

   signals:
      void clientStatusChanged(const ClientStatus& clientStatus);

   private:
      std::string displayName_;
      ClientStatus clientStatus_;
   };

   using ClientPartyPtr = std::shared_ptr<ClientParty>;

}

Q_DECLARE_METATYPE(Chat::ClientPartyPtr)

#endif // ClientParty_h__
