#ifndef PARTYMODEL_H
#define PARTYMODEL_H

#include <QObject>
#include <memory.h>
#include <unordered_map>
#include <vector>

#include "ChatProtocol/Party.h"
#include "ChatProtocol/PrivateDirectMessageParty.h"

namespace spdlog
{
   class logger;
}

namespace Chat
{
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
      PartyModel(const LoggerPtr& loggerPtr, QObject* parent = nullptr);

      void insertParty(const PartyPtr& partyPtr);
      void removeParty(const PartyPtr& partyPtr);
      PartyPtr getPartyById(const std::string& party_id);
      PrivateDirectMessagePartyPtr getPrivatePartyById(const std::string& party_id);
      void clearModel();
      void insertOrUpdateParty(const PartyPtr& partyPtr);

   signals:
      void partyInserted(const Chat::PartyPtr& partyPtr);
      void partyRemoved(const Chat::PartyPtr& partyPtr);
      void error(const Chat::PartyModelError& errorCode, const std::string& what = "");
      void partyModelChanged();

   private slots:
      void handleLocalErrors(const Chat::PartyModelError& errorCode, const std::string& what = "");

   protected:
      PartyMap partyMap_;
      LoggerPtr loggerPtr_;
   };

   using PartyModelPtr = std::shared_ptr<PartyModel>;

}

#endif // PARTYMODEL_H
