#ifndef PARTYRECIPIENT_H
#define PARTYRECIPIENT_H

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
      PartyRecipient(std::string userHash, BinaryData publicKey = BinaryData(), QDateTime publicKeyTime = QDateTime::currentDateTime());

      std::string userHash() const { return userHash_; }
      void setUserHash(const std::string& val) { userHash_ = val; }

      BinaryData publicKey() const { return publicKey_; }
      void setPublicKey(const BinaryData& val) { publicKey_ = val; }

      QDateTime publicKeyTime() const { return publicKeyTime_; }
      void setPublicKeyTime(const QDateTime& val) { publicKeyTime_ = val; }

      CelerClient::CelerUserType celerType() const { return celerType_; }
      void setCelerType(const CelerClient::CelerUserType& celerType) { celerType_ = celerType; }

   private:
      std::string userHash_;
      BinaryData publicKey_;
      QDateTime publicKeyTime_;
      CelerClient::CelerUserType celerType_ = bs::network::UserType::Undefined;
   };

   using PartyRecipientPtr = std::shared_ptr<PartyRecipient>;
   using PartyRecipientsPtrList = std::vector<PartyRecipientPtr>;
   using UniqieRecipientMap = std::unordered_map<std::string, PartyRecipientPtr>;

}

#endif // PARTYRECIPIENT_H
