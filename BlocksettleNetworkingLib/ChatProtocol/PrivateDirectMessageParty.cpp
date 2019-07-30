#include "PrivateDirectMessageParty.h"

namespace Chat
{

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