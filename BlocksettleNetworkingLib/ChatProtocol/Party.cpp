#include "Party.h"

#include <QUuid>

using namespace Chat;

Party::Party()
{
   // default states
   std::string uuid = QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
   setId(uuid);
   setPartyType(PartyType::GLOBAL);
   setPartySubType(PartySubType::STANDARD);
}

Party::Party(const PartyType& partyType, const PartySubType& partySubType, const PartyState& partyState)
{
   setId(QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString());
   setPartyType(partyType);
   setPartySubType(partySubType);
   setPartyState(partyState);
}

Party::Party(const std::string& id, const PartyType& partyType, const PartySubType& partySubType, const PartyState& partyState)
{
   setId(id);
   setPartyType(partyType);
   setPartySubType(partySubType);
   setPartyState(partyState);
}
