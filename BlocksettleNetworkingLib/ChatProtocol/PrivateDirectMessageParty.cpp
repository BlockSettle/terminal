#include "PrivateDirectMessageParty.h"

namespace Chat
{

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

   // TODO: In case of performance problems in chat server 
   //    consider better solution to find and return all recipients different than given user
   PartyRecipientPtr PrivateDirectMessageParty::getSecondRecipient(const std::string& firstRecipientUserName)
   {
      PartyRecipientPtr found;

      for (const auto& recipient : recipients_)
      {
         if (recipient->userName() == firstRecipientUserName)
         {
            continue;
         }

         found = recipient;
      }

      return found;
   }

   PartyRecipientsPtrList PrivateDirectMessageParty::getRecipientsExceptMe(const std::string& me)
   {
      PartyRecipientsPtrList recipients;
      for (const auto& recipient : recipients_)
      {
         if (recipient->userName() == me)
         {
            continue;
         }

         recipients.push_back(recipient);
      }

      return recipients;
   }

}