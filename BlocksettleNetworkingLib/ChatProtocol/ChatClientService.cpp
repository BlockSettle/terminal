#include "ChatProtocol/ChatClientService.h"

namespace Chat
{

   ChatClientService::ChatClientService(QObject* parent /*= nullptr*/)
      : ServiceThread<ChatClientLogic>(new ChatClientLogic, parent)
   {
      qRegisterMetaType<ConnectionManagerPtr>();
      qRegisterMetaType<ApplicationSettingsPtr>();
      qRegisterMetaType<LoggerPtr>();
      qRegisterMetaType<ZmqBIP15XDataConnection::cbNewKey>();

      /////////// INPUT SIGNALS //////////////
      connect(this, &ChatClientService::Init, worker(), &ChatClientLogic::Init);
      connect(this, &ChatClientService::LoginToServer, worker(), &ChatClientLogic::LoginToServer);

      /////////// OUTPUT SIGNALS //////////////
      connect(worker(), &ChatClientLogic::chatUserDisplayNameChanged, this, &ChatClientService::chatUserDisplayNameChanged);
      connect(worker(), &ChatClientLogic::error, this, &ChatClientService::error);
   }

}