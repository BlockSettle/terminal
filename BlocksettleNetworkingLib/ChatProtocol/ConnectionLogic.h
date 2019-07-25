#ifndef ConnectionLogic_h__
#define ConnectionLogic_h__

#include <memory>
#include <QObject>

#include "DataConnectionListener.h"

namespace spdlog
{
   class logger;
}

namespace Chat
{
   using LoggerPtr = std::shared_ptr<spdlog::logger>;

   class ConnectionLogic : public QObject
   {
      Q_OBJECT
   public:
      explicit ConnectionLogic(const LoggerPtr& loggerPtr, QObject* parent = nullptr);

   public slots:
      void onDataReceived(const std::string&);
      void onConnected(void);
      void onDisconnected(void);
      void onError(DataConnectionListener::DataConnectionError);

   private:
      LoggerPtr   loggerPtr_;
   };

   using ConnectionLogicPtr = std::shared_ptr<ConnectionLogic>;
}

#endif // ConnectionLogic_h__
