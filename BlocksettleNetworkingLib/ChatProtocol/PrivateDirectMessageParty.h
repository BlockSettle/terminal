/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef PRIVATEDIRECTMESSAGEPARTY_H
#define PRIVATEDIRECTMESSAGEPARTY_H

#include <QMetaType>

#include "ChatProtocol/Party.h"
#include "ChatProtocol/PartyRecipient.h"

#include <memory>
#include <vector>

#include "chat.pb.h"

namespace Chat
{
   class PrivateDirectMessageParty : public Party
   {
   public:
      PrivateDirectMessageParty(
         const PartyType& partyType = PRIVATE_DIRECT_MESSAGE, 
         const PartySubType& partySubType = STANDARD, 
         const PartyState& partyState = UNINITIALIZED
      );

      PrivateDirectMessageParty(
         const std::string& id, 
         const PartyType& partyType = PRIVATE_DIRECT_MESSAGE, 
         const PartySubType& partySubType = STANDARD, 
         const PartyState& partyState = UNINITIALIZED
      );

      PartyRecipientsPtrList recipients() const { return recipients_; }
      void setRecipients(const PartyRecipientsPtrList& val) { recipients_ = val; }

      void insertOrUpdateRecipient(const PartyRecipientPtr& partyRecipientPtr);
      PartyRecipientPtr getRecipient(const std::string& recipientUserHash);

      bool isUserBelongsToParty(const std::string& recipientUserHash);
      bool isUserInPartyWith(const std::string& firstUserHash, const std::string& secondUserHash);
      PartyRecipientPtr getSecondRecipient(const std::string& firstRecipientUserHash);
      PartyRecipientsPtrList getRecipientsExceptMe(const std::string& myUserHash);

   private:
      PartyRecipientsPtrList recipients_;
   };

   using PrivateDirectMessagePartyPtr = std::shared_ptr<PrivateDirectMessageParty>;

}

Q_DECLARE_METATYPE(Chat::PrivateDirectMessagePartyPtr)

#endif // PRIVATEDIRECTMESSAGEPARTY_H
