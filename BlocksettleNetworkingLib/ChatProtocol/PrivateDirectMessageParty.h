#ifndef PRIVATEDIRECTMESSAGEPARTY_H
#define PRIVATEDIRECTMESSAGEPARTY_H

#include <QMetaType>

#include "ChatProtocol/Party.h"
#include "ChatProtocol/PartyRecipient.h"

#include <memory>
#include <vector>

namespace Chat
{
   class PrivateDirectMessageParty : public Party
   {
   public:
      PrivateDirectMessageParty(
         const PartyType& partyType = PartyType::PRIVATE_DIRECT_MESSAGE, 
         const PartySubType& partySubType = PartySubType::STANDARD, 
         const PartyState& partyState = PartyState::UNINITIALIZED
      );

      PrivateDirectMessageParty(
         const std::string& id, 
         const PartyType& partyType = PartyType::PRIVATE_DIRECT_MESSAGE, 
         const PartySubType& partySubType = PartySubType::STANDARD, 
         const PartyState& partyState = PartyState::UNINITIALIZED
      );

      PartyRecipientsPtrList recipients() const { return recipients_; }
      void setRecipients(PartyRecipientsPtrList val) { recipients_ = val; }

      void insertOrUpdateRecipient(const PartyRecipientPtr& partyRecipientPtr);
      PartyRecipientPtr getRecipient(const std::string& userName);

      bool isUserBelongsToParty(const std::string& userName);
      PartyRecipientPtr getSecondRecipient(const std::string& firstRecipientUserName);
      PartyRecipientsPtrList getRecipientsExceptMe(const std::string& me);

   private:
      PartyRecipientsPtrList recipients_;
   };

   using PrivateDirectMessagePartyPtr = std::shared_ptr<PrivateDirectMessageParty>;

}

Q_DECLARE_METATYPE(Chat::PrivateDirectMessagePartyPtr)
Q_DECLARE_METATYPE(Chat::PartyRecipientsPtrList)

#endif // PRIVATEDIRECTMESSAGEPARTY_H
