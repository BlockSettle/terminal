#ifndef ChatClientService_h__
#define ChatClientService_h__

#include <memory>

#include <QObject>

#include "ChatProtocol/ServiceThread.h"
#include "ChatProtocol/ChatClientLogic.h"

namespace Chat
{

   class ChatClientService : public ServiceThread<ChatClientLogic>
   {
      Q_OBJECT

   public:
      explicit ChatClientService(QObject* parent = nullptr);

      ClientPartyModelPtr getClientPartyModelPtr();

   signals:
      ////////// PROXY SIGNALS //////////
      void Init(const Chat::ConnectionManagerPtr& connectionManagerPtr, const Chat::ApplicationSettingsPtr& appSettings, const Chat::LoggerPtr& loggerPtr);
      void LoginToServer(const std::string& email, const std::string& jwt, const ZmqBipNewKeyCb& cb);
      void LogoutFromServer();
      void InitParty(const std::string& partyId);

      ////////// RETURN SIGNALS //////////
      void chatUserDisplayNameChanged(const std::string& chatUserDisplayName);
      void chatClientError(const Chat::ChatClientLogicError& errorCode);
      void clientLoggedOutFromServer();
      void partyModelChanged();
   };

   using ChatClientServicePtr = std::shared_ptr<ChatClientService>;

}

Q_DECLARE_METATYPE(Chat::ConnectionManagerPtr)
Q_DECLARE_METATYPE(Chat::ApplicationSettingsPtr)
Q_DECLARE_METATYPE(Chat::LoggerPtr)
Q_DECLARE_METATYPE(ZmqBipNewKeyCb)

#endif // ChatClientService_h__
