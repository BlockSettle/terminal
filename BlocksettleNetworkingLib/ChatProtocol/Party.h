#ifndef PARTY_H
#define PARTY_H

#include <QMetaType>

#include <memory>
#include <string>
#include <vector>

#include "chat.pb.h"

namespace Chat
{

   class Party
   {
   public:
      Party();
      Party(const PartyType& partyType, const PartySubType& partySubType = PartySubType::STANDARD, const PartyState& partyState = PartyState::UNINITIALIZED);
      Party(const std::string& id, const PartyType& partyType, const PartySubType& partySubType = PartySubType::STANDARD, const PartyState& partyState = PartyState::UNINITIALIZED);

      virtual ~Party() {}

      virtual std::string id() const { return id_; }
      virtual void setId(std::string val) { id_ = val; }

      virtual Chat::PartyType partyType() const { return partyType_; }
      virtual void setPartyType(Chat::PartyType val) { partyType_ = val; }

      virtual Chat::PartySubType partySubType() const { return partySubType_; }
      virtual void setPartySubType(Chat::PartySubType val) { partySubType_ = val; }

      virtual Chat::PartyState partyState() const { return partyState_; }
      virtual void setPartyState(Chat::PartyState val) { partyState_ = val; }

      virtual bool isGlobalStandard() const { return (Chat::PartyType::GLOBAL == partyType() && Chat::PartySubType::STANDARD == partySubType()); }
      virtual bool isGlobalOTC() const { return (Chat::PartyType::GLOBAL == partyType() && Chat::PartySubType::OTC == partySubType()); }
      virtual bool isGlobal() const { return Chat::PartyType::GLOBAL == partyType(); }
      virtual bool isPrivateStandard() const { return (Chat::PartyType::PRIVATE_DIRECT_MESSAGE == partyType() && Chat::PartySubType::STANDARD == partySubType()); }
      virtual bool isPrivate() const { return Chat::PartyType::PRIVATE_DIRECT_MESSAGE == partyType(); }

      const std::string& partyCreatorHash() const { return partyCreatorHash_; }
      void setPartyCreatorHash(std::string val) { partyCreatorHash_ = val; }

   private:
      std::string id_;
      PartyType partyType_;
      PartySubType partySubType_;
      PartyState partyState_;
      std::string partyCreatorHash_;
   };

   using PartyPtr = std::shared_ptr<Party>;
   using PartyPtrList = std::vector<PartyPtr>;
}

Q_DECLARE_METATYPE(Chat::PartyPtrList)

#endif // PARTY_H
