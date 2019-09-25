#include "ChatProtocol/ClientParty.h"

using namespace Chat;

const char *Chat::GlobalRoomName = "Global";
const char *Chat::OtcRoomName = "OTC";
const char *Chat::SupportRoomName = "Support";

ClientParty::ClientParty(
   const std::string& id, const PartyType& partyType, const PartySubType& partySubType,
   const PartyState& partyState, QObject* parent)
   : QObject(parent), PrivateDirectMessageParty(id, partyType, partySubType, partyState), clientStatus_(ClientStatus::OFFLINE)
{
}

void ClientParty::setClientStatus(ClientStatus val)
{
   clientStatus_ = val;

   emit clientStatusChanged(clientStatus_);
}

void ClientParty::setDisplayName(std::string val)
{
   displayName_ = val;
   emit displayNameChanged();
}

void ClientParty::setPartyState(Chat::PartyState val)
{
   Party::setPartyState(val);
   emit partyStateChanged(id());
}
