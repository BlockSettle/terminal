/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
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
      explicit Party(const PartyType& partyType, const PartySubType& partySubType = STANDARD, const PartyState& partyState = UNINITIALIZED);
      Party(const std::string& id, const PartyType& partyType, const PartySubType& partySubType = STANDARD, const PartyState& partyState = UNINITIALIZED);

      virtual ~Party() = default;

      Party(const Party&) = default;
      Party& operator=(const Party&) = default;
      Party(Party&&) = default;
      Party& operator=(Party&&) = default;

      virtual std::string id() const { return id_; }
      virtual void setId(const std::string& val) { id_ = val; }

      virtual PartyType partyType() const { return partyType_; }
      virtual void setPartyType(const PartyType& val) { partyType_ = val; }

      virtual PartySubType partySubType() const { return partySubType_; }
      virtual void setPartySubType(const PartySubType& val) { partySubType_ = val; }

      virtual PartyState partyState() const { return partyState_; }
      virtual void setPartyState(const PartyState& val) { partyState_ = val; }

      virtual bool isGlobalStandard() const { return (GLOBAL == partyType() && STANDARD == partySubType()); }
      virtual bool isGlobalOTC() const { return (GLOBAL == partyType() && OTC == partySubType()); }
      virtual bool isGlobal() const { return GLOBAL == partyType(); }
      virtual bool isPrivateStandard() const { return (PRIVATE_DIRECT_MESSAGE == partyType() && STANDARD == partySubType()); }
      virtual bool isPrivateOTC() const { return (PRIVATE_DIRECT_MESSAGE == partyType() && OTC == partySubType()); }
      virtual bool isPrivate() const { return PRIVATE_DIRECT_MESSAGE == partyType(); }

      const std::string& partyCreatorHash() const { return partyCreatorHash_; }
      void setPartyCreatorHash(const std::string& val) { partyCreatorHash_ = val; }

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
