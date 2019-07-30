#ifndef PrivateDirectMessageParty_h__
#define PrivateDirectMessageParty_h__

#include "Party.h"

#include <memory>
#include <vector>

namespace Chat
{
   using Recipients = std::vector<std::string>;

   class PrivateDirectMessageParty : public Party
   {
   public:
      Recipients recipients() const { return recipients_; }
      void setRecipients(Recipients val) { recipients_ = val; }

      bool isUserBelongsToParty(const std::string& userName);
      std::string getSecondRecipient(const std::string& firstRecipientUserName);

   private:
      Recipients recipients_;
   };

   using PrivateDirectMessagePartyPtr = std::shared_ptr<PrivateDirectMessageParty>;

}

#endif // PrivateDirectMessageParty_h__