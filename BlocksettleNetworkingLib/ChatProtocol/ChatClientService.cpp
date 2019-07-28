#include "ChatProtocol/ChatClientService.h"

namespace Chat
{

   ChatClientService::ChatClientService(QObject* parent /*= nullptr*/)
      : ServiceThread<ChatClientLogic>(new ChatClientLogic, parent)
   {
      qRegisterMetaType<ConnectionManagerPtr>();
      qRegisterMetaType<ApplicationSettingsPtr>();
      qRegisterMetaType<LoggerPtr>();
      qRegisterMetaType<ZmqBipNewKeyCb>();
      
      ////////// PROXY SIGNALS //////////
      connect(this, &ChatClientService::Init, worker(), &ChatClientLogic::Init);
      connect(this, &ChatClientService::LoginToServer, worker(), &ChatClientLogic::LoginToServer);

      ////////// RETURN SIGNALS //////////
      connect(worker(), &ChatClientLogic::chatUserDisplayNameChanged, this, &ChatClientService::chatUserDisplayNameChanged);
      connect(worker(), &ChatClientLogic::chatClientError, this, &ChatClientService::chatClientError);
   }

}