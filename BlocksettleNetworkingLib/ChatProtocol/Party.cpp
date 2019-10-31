#include "Party.h"

#include <QUuid>

using namespace Chat;

Party::Party()
   : partyType_(GLOBAL),
   partySubType_(STANDARD),
   partyState_(UNINITIALIZED)
{
   // default states
   Party::setId(QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString());
}

Party::Party(const PartyType& partyType, const PartySubType& partySubType, const PartyState& partyState)
   : partyType_(partyType),
   partySubType_(partySubType),
   partyState_(partyState)

{
   Party::setId(QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString());
}

Party::Party(const std::string& id, const PartyType& partyType, const PartySubType& partySubType, const PartyState& partyState)
   : partyType_(partyType),
   partySubType_(partySubType),
   partyState_(partyState)
{
   Party::setId(id);
}
