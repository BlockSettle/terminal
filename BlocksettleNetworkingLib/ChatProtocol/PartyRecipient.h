#ifndef PARTYRECIPIENT_H
#define PARTYRECIPIENT_H

#include <QDateTime>

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
      PartyRecipient(const std::string& userName, const BinaryData& publicKey = BinaryData(), const QDateTime& publicKeyTime = QDateTime::currentDateTime());

      std::string userName() const { return userName_; }
      void setUserName(std::string val) { userName_ = val; }

      BinaryData publicKey() const { return publicKey_; }
      void setPublicKey(BinaryData val) { publicKey_ = val; }

      QDateTime publicKeyTime() const { return publicKeyTime_; }
      void setPublicKeyTime(QDateTime val) { publicKeyTime_ = val; }

   private:
      std::string userName_;
      BinaryData publicKey_;
      QDateTime publicKeyTime_;
   };

   using PartyRecipientPtr = std::shared_ptr<PartyRecipient>;
   using PartyRecipientsPtrList = std::vector<PartyRecipientPtr>;

}

#endif // PARTYRECIPIENT_H
