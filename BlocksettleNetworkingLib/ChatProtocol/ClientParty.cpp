#include "ChatProtocol/ClientParty.h"

namespace Chat
{

   ClientParty::ClientParty(
      const std::string& id, const PartyType& partyType, const PartySubType& partySubType, 
      const PartyState& partyState, QObject* parent)
      : QObject(parent), Party(id, partyType, partySubType, partyState), clientStatus_(ClientStatus::OFFLINE)
   {
   }

   void ClientParty::setClientStatus(ClientStatus val)
   {
      clientStatus_ = val;

      emit clientStatusChanged(clientStatus_);
   }

}
