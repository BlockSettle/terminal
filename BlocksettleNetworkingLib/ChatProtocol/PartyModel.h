/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef PARTYMODEL_H
#define PARTYMODEL_H

#include <QObject>
#include <unordered_map>
#include <vector>
#include <atomic>

#include "ChatProtocol/Party.h"
#include "ChatProtocol/PrivateDirectMessageParty.h"

namespace spdlog
{
   class logger;
}

namespace Chat
{
   namespace ErrorType {
      const std::string ErrorDescription = "Error";
      const std::string WarningDescription = "Warning";
   }

   using PartyMap = std::unordered_map<std::string, PartyPtr>;
   using LoggerPtr = std::shared_ptr<spdlog::logger>;
   using IdPartyList = std::vector<std::string>;

   enum class PartyModelError
   {
      InsertExistingParty,
      RemovingNonexistingParty,
      CouldNotFindParty,
      DynamicPointerCast,
      PrivatePartyCasting
   };

   class PartyModel : public QObject
   {
      Q_OBJECT
   public:
      PartyModel(LoggerPtr loggerPtr, QObject* parent = nullptr);

      void insertParty(const PartyPtr& partyPtr);
      void removeParty(const PartyPtr& partyPtr);
      PartyPtr getPartyById(const std::string& party_id);
      PrivateDirectMessagePartyPtr getPrivatePartyById(const std::string& party_id);
      void clearModel();
      void insertOrUpdateParty(const PartyPtr& partyPtr);

   signals:
      void partyInserted(const Chat::PartyPtr& partyPtr);
      void partyRemoved(const Chat::PartyPtr& partyPtr);
      void error(const Chat::PartyModelError& errorCode, const std::string& what = "", bool displayAsWarning = false);
      void partyModelChanged();

   private slots:
      void handleLocalErrors(const Chat::PartyModelError& errorCode, const std::string& what = "", bool displayAsWarning = false) const;

   protected:
      PartyMap partyMap_;
      LoggerPtr loggerPtr_;
      std::atomic_flag partyMapLockerFlag_ = ATOMIC_FLAG_INIT;
   };

   using PartyModelPtr = std::shared_ptr<PartyModel>;
}

Q_DECLARE_METATYPE(Chat::PartyModelError)

#endif // PARTYMODEL_H
