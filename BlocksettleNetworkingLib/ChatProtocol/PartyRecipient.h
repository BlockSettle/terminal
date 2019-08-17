#ifndef PartyRecipient_h__
#define PartyRecipient_h__

#include <memory>
#include <vector>

#include <disable_warnings.h>
#include "BinaryData.h"
#include <enable_warnings.h>

namespace Chat
{

   class PartyRecipient
   {
   public:
      PartyRecipient(const std::string& userName, const BinaryData& publicKey = BinaryData());

      std::string userName() const { return userName_; }
      void setUserName(std::string val) { userName_ = val; }

      BinaryData publicKey() const { return publicKey_; }
      void setPublicKey(BinaryData val) { publicKey_ = val; }

   private:
      std::string userName_;
      BinaryData publicKey_;
   };

   using PartyRecipientPtr = std::shared_ptr<PartyRecipient>;
   using PartyRecipientsPtrList = std::vector<PartyRecipientPtr>;

}

#endif // PartyRecipient_h__
