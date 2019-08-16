#include "ClientPrivateDMParty.h"

namespace Chat
{

   ClientPrivateDMParty::ClientPrivateDMParty(const std::string& id, const PartyType& partyType, const PartySubType& partySubType, const PartyState& partyState, QObject* parent /* = nullptr */)
      : ClientParty(id, partyType, partySubType, partyState, parent),
      PrivateDirectMessageParty(partyType, partySubType, partyState)
   {

   }
}