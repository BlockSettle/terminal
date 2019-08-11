#include "ChatProtocol/ClientDBService.h"

namespace Chat
{

   ClientDBService::ClientDBService(QObject* parent /* = nullptr */)
      : ServiceThread<ClientDBLogic>(new ClientDBLogic, parent)
   {
      qRegisterMetaType<Chat::ApplicationSettingsPtr>();

      ////////// PROXY SIGNALS //////////
      connect(this, &ClientDBService::Init, worker(), &ClientDBLogic::Init);

      ////////// RETURN SIGNALS //////////
      connect(worker(), &ClientDBLogic::initDone, this, &ClientDBService::initDone);
   }
}