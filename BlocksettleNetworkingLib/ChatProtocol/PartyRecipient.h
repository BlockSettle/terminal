#ifndef PARTYRECIPIENT_H
#define PARTYRECIPIENT_H

#include <QDateTime>

#include <unordered_map>
#include <memory>
#include <vector>

#include "CelerClient.h"
#include <disable_warnings.h>
#include "BinaryData.h"
#include <enable_warnings.h>

namespace Chat
{

   class PartyRecipient
   {
   public:
      PartyRecipient(const std::string& userHash, const BinaryData& publicKey = BinaryData(), const QDateTime& publicKeyTime = QDateTime::currentDateTime());

      std::string userHash() const { return userHash_; }
      void setUserHash(std::string val) { userHash_ = val; }

      BinaryData publicKey() const { return publicKey_; }
      void setPublicKey(BinaryData val) { publicKey_ = val; }

      QDateTime publicKeyTime() const { return publicKeyTime_; }
      void setPublicKeyTime(QDateTime val) { publicKeyTime_ = val; }

      CelerClient::CelerUserType celerType() const { return celerType_; }
      void setCelerType(CelerClient::CelerUserType celerType) { celerType_ = celerType; }

   private:
      std::string userHash_;
      BinaryData publicKey_;
      QDateTime publicKeyTime_;
      CelerClient::CelerUserType celerType_ = CelerClient::Undefined;
   };

   using PartyRecipientPtr = std::shared_ptr<PartyRecipient>;
   using PartyRecipientsPtrList = std::vector<PartyRecipientPtr>;
   using UniqieRecipientMap = std::unordered_map<std::string, PartyRecipientPtr>;

}

#endif // PARTYRECIPIENT_H
