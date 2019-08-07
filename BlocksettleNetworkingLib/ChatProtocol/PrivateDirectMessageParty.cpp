#include "PrivateDirectMessageParty.h"

namespace Chat
{
   PrivateDirectMessageParty::PrivateDirectMessageParty(const std::string& id, const PartyType& partyType, const PartySubType& partySubType, const PartyState& partyState)
      : Party(id, partyType, partySubType, partyState)
   {

   }

   bool PrivateDirectMessageParty::isUserBelongsToParty(const std::string& userName)
   {
      for (const auto& recipientUserName : recipients_)
      {
         if (recipientUserName == userName)
         {
            return true;
         }
      }

      return false;
   }

   // TODO: In case of performance problems in chat server 
   //    consider better solution to find and return all recipients different than given user
   std::string PrivateDirectMessageParty::getSecondRecipient(const std::string& firstRecipientUserName)
   {
      std::string found;

      for (const auto& recipientUserName : recipients_)
      {
         if (recipientUserName == firstRecipientUserName)
         {
            continue;
         }

         found = recipientUserName;
      }

      return found;
   }
}