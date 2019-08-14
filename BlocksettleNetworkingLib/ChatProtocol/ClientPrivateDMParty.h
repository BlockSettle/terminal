#ifndef ClientPrivateDMParty_h__
#define ClientPrivateDMParty_h__

#include <memory>

#include "ChatProtocol/ClientParty.h"
#include "ChatProtocol/PrivateDirectMessageParty.h"

namespace Chat
{

   class ClientPrivateDMParty : public ClientParty, public PrivateDirectMessageParty
   {
      Q_OBJECT
   public:
      ClientPrivateDMParty(
         const std::string& id, 
         const PartyType& partyType = PartyType::PRIVATE_DIRECT_MESSAGE, 
         const PartySubType& partySubType = PartySubType::STANDARD, 
         const PartyState& partyState = PartyState::UNINITIALIZED, 
         QObject* parent = nullptr
      );
   };

   using ClientPrivateDMPartyPtr = std::shared_ptr<ClientPrivateDMParty>;
}

#endif // ClientPrivateDMParty_h__
