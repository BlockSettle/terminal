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

bool PrivateDirectMessageParty::isUserBelongsToParty(const std::string& userName)
{
   for (const auto& recipient : recipients_)
   {
      if (recipient->userName() == userName)
      {
         return true;
      }
   }

   return false;
}

PartyRecipientPtr PrivateDirectMessageParty::getSecondRecipient(const std::string& firstRecipientUserName)
{
   PartyRecipientsPtrList recipients = getRecipientsExceptMe(firstRecipientUserName);

   if (recipients.empty())
   {
      return nullptr;
   }

   return recipients.back();
}

PartyRecipientsPtrList PrivateDirectMessageParty::getRecipientsExceptMe(const std::string& me)
{
   PartyRecipientsPtrList recipients;

   std::copy_if(recipients_.begin(), recipients_.end(), std::back_inserter(recipients),
      [me](const PartyRecipientPtr& existing) {
         return existing->userName() != me;
      });

   return recipients;
}

void PrivateDirectMessageParty::insertOrUpdateRecipient(const PartyRecipientPtr& partyRecipientPtr)
{
   PartyRecipientPtr recipientPtr = getRecipient(partyRecipientPtr->userName());

   if (nullptr == recipientPtr)
   {
      // not found, add new
      recipients_.push_back(partyRecipientPtr);
      return;
   }

   // found, update
   recipientPtr->setPublicKey(partyRecipientPtr->publicKey());
}

PartyRecipientPtr PrivateDirectMessageParty::getRecipient(const std::string& userName)
{
   for (const auto& recipient : recipients_)
   {
      if (recipient->userName() == userName)
      {
         return recipient;
      }
   }

   return nullptr;
}

