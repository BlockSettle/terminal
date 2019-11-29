/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "PrivateDirectMessageParty.h"

using namespace Chat;

PrivateDirectMessageParty::PrivateDirectMessageParty(const PartyType& partyType, const PartySubType& partySubType, const PartyState& partyState)
   : Party(partyType, partySubType, partyState)
{

}

PrivateDirectMessageParty::PrivateDirectMessageParty(const std::string& id, const PartyType& partyType, const PartySubType& partySubType, const PartyState& partyState)
   : Party(id, partyType, partySubType, partyState)
{

}

bool PrivateDirectMessageParty::isUserBelongsToParty(const std::string& recipientUserHash)
{
   const auto& it = std::find_if(recipients_.begin(), recipients_.end(), [recipientUserHash](const auto& recipient)->bool {
      return recipient->userHash() == recipientUserHash;
   });

   return it != recipients_.end();
}

bool PrivateDirectMessageParty::isUserInPartyWith(const std::string& firstUserHash, const std::string& secondUserHash)
{
   return isUserBelongsToParty(firstUserHash) && isUserBelongsToParty(secondUserHash);
}

// TODO: consider to remove this function
PartyRecipientPtr PrivateDirectMessageParty::getSecondRecipient(const std::string& firstRecipientUserHash)
{
   const auto recipients = getRecipientsExceptMe(firstRecipientUserHash);

   if (recipients.empty())
   {
      return nullptr;
   }

   return recipients.back();
}

PartyRecipientsPtrList PrivateDirectMessageParty::getRecipientsExceptMe(const std::string& myUserHash)
{
   PartyRecipientsPtrList recipients;

   std::copy_if(recipients_.begin(), recipients_.end(), std::back_inserter(recipients),
      [myUserHash](const PartyRecipientPtr& existing) {
         return existing->userHash() != myUserHash;
      });

   return recipients;
}

void PrivateDirectMessageParty::insertOrUpdateRecipient(const PartyRecipientPtr& partyRecipientPtr)
{
   auto recipientPtr = getRecipient(partyRecipientPtr->userHash());

   if (nullptr == recipientPtr)
   {
      // not found, add new
      recipients_.push_back(partyRecipientPtr);
      return;
   }

   // found, update
   recipientPtr->setPublicKey(partyRecipientPtr->publicKey());
}

PartyRecipientPtr PrivateDirectMessageParty::getRecipient(const std::string& recipientUserHash)
{
   for (const auto& recipient : recipients_)
   {
      if (recipient->userHash() == recipientUserHash)
      {
         return recipient;
      }
   }

   return nullptr;
}

