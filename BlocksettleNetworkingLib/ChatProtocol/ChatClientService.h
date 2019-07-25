#ifndef ChatClientService_h__
#define ChatClientService_h__

#include <memory>

#include <QObject>

#include "ChatProtocol/ServiceThread.h"
#include "ChatProtocol/ChatClientLogic.h"

namespace Chat
{
   Q_DECLARE_METATYPE(ConnectionManagerPtr)
   Q_DECLARE_METATYPE(ApplicationSettingsPtr)
   Q_DECLARE_METATYPE(LoggerPtr)
   Q_DECLARE_METATYPE(ZmqBIP15XDataConnection::cbNewKey);

   class ChatClientService : public ServiceThread<ChatClientLogic>
   {
      Q_OBJECT
         
   public:
      explicit ChatClientService(QObject* parent = nullptr);

   signals:
      ////////// PROXY SIGNALS //////////
      void Init(const ConnectionManagerPtr& connectionManagerPtr, const ApplicationSettingsPtr& appSettings, const LoggerPtr& loggerPtr);
      void LoginToServer(const std::string& email, const std::string& jwt, const ZmqBIP15XDataConnection::cbNewKey& cb);

      ////////// RETURN SIGNALS //////////
      void chatUserDisplayNameChanged(const std::string& chatUserDisplayName);
      void chatClientError(const ChatClientLogicError& errorCode);
   };

   using ChatClientServicePtr = std::shared_ptr<ChatClientService>;

}

#endif // ChatClientService_h__
