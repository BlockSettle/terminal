#include "ChatProtocol/ClientParty.h"

namespace Chat
{

   ClientParty::ClientParty(const std::string& id, const PartyType& partyType, const PartySubType& partySubType, QObject* parent)
      : QObject(parent), Party(id, partyType, partySubType)
   {
      setClientStatus(ClientStatus::OFFLINE);
   }

   void ClientParty::setClientStatus(ClientStatus val)
   {
      clientStatus_ = val;

      emit clientStatusChanged(clientStatus_);
   }

}
