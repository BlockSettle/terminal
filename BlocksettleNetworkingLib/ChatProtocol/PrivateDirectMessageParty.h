#ifndef PrivateDirectMessageParty_h__
#define PrivateDirectMessageParty_h__

#include <QMetaType>

#include "Party.h"

#include <memory>
#include <vector>

namespace Chat
{
   using Recipients = std::vector<std::string>;

   class PrivateDirectMessageParty : public virtual Party
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

      Recipients recipients() const { return recipients_; }
      void setRecipients(Recipients val) { recipients_ = val; }

      bool isUserBelongsToParty(const std::string& userName);
      std::string getSecondRecipient(const std::string& firstRecipientUserName);
      Recipients getRecipientsExceptMe(const std::string& me);

   private:
      Recipients recipients_;
   };

   using PrivateDirectMessagePartyPtr = std::shared_ptr<PrivateDirectMessageParty>;

}

Q_DECLARE_METATYPE(Chat::PrivateDirectMessagePartyPtr)

#endif // PrivateDirectMessageParty_h__