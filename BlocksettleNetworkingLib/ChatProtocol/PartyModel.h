#ifndef PartyModel_h__
#define PartyModel_h__

#include <QObject>
#include <memory.h>
#include <unordered_map>
#include <vector>

#include "ChatProtocol/Party.h"

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
      DynamicPointerCast
   };

   class PartyModel : public QObject
   {
      Q_OBJECT
   public:
      PartyModel(const LoggerPtr& loggerPtr, QObject* parent = nullptr);

      void insertParty(const PartyPtr& partyPtr);
      void removeParty(const PartyPtr& partyPtr);
      PartyPtr getPartyById(const std::string& id);

   signals:
      void partyInserted(const PartyPtr& partyPtr);
      void partyRemoved(const PartyPtr& partyPtr);
      void error(const PartyModelError& errorCode, const std::string& id = "");

   private slots:
      void handleLocalErrors(const PartyModelError& errorCode, const std::string& id = "");

   protected:
      PartyMap partyMap_;
      LoggerPtr loggerPtr_;
   };

   using PartyModelPtr = std::shared_ptr<PartyModel>;

}

#endif // PartyModel_h__
