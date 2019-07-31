#ifndef ClientPartyModel_h__
#define ClientPartyModel_h__

#include <QObject>
#include <memory>
#include <unordered_map>
#include <vector>

#include "ChatProtocol/PartyModel.h"

namespace spdlog
{
   class logger;
}

namespace Chat
{
   using LoggerPtr = std::shared_ptr<spdlog::logger>;

   class ClientPartyModel : public PartyModel
   {
      Q_OBJECT
   public:
      ClientPartyModel(const LoggerPtr& loggerPtr, QObject* parent = nullptr);

   };

   using ClientPartyModelPtr = std::shared_ptr<ClientPartyModel>;

}

#endif // ClientPartyModel_h__
