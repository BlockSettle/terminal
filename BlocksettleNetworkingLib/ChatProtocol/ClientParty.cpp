/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "ChatProtocol/ClientParty.h"

using namespace Chat;

const char* Chat::GlobalRoomName = "Global";
const char* Chat::OtcRoomName = "OTC";
const char* Chat::SupportRoomName = "Support";

ClientParty::ClientParty(
   const std::string& id, const PartyType& partyType, const PartySubType& partySubType,
   const PartyState& partyState, QObject* parent)
   : QObject(parent), PrivateDirectMessageParty(id, partyType, partySubType, partyState), clientStatus_(OFFLINE)
{
}

void ClientParty::setClientStatus(const ClientStatus& val)
{
   clientStatus_ = val;

   emit clientStatusChanged(clientStatus_);
}

void ClientParty::setDisplayName(const std::string& val)
{
   displayName_ = val;
   emit displayNameChanged();
}

void ClientParty::setPartyState(const PartyState& val)
{
   Party::setPartyState(val);
   emit partyStateChanged(id());
}
