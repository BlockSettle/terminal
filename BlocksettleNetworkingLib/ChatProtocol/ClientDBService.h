#ifndef ClientDBService_h__
#define ClientDBService_h__

#include <QObject>
#include <memory>

#include "ChatProtocol/ServiceThread.h"
#include "ChatProtocol/ClientDBLogic.h"

namespace spdlog
{
   class logger;
}

namespace Chat
{

   class ClientDBService : public ServiceThread<ClientDBLogic>
   {
      Q_OBJECT

   public:
      ClientDBService(QObject* parent = nullptr);

   signals:
      ////////// PROXY SIGNALS //////////
      void Init(const Chat::LoggerPtr& loggerPtr, const ApplicationSettingsPtr& appSettings);

      ////////// RETURN SIGNALS //////////
      void initDone();
   };
}

#endif // ClientDBService_h__
