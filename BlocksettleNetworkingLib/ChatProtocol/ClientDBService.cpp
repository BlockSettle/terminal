#include "ChatProtocol/ClientDBService.h"

namespace Chat
{

   ClientDBService::ClientDBService(QObject* parent /* = nullptr */)
      : ServiceThread<ClientDBLogic>(new ClientDBLogic, parent)
   {
      ////////// PROXY SIGNALS //////////
      connect(this, &ClientDBService::Init, worker(), &ClientDBLogic::Init);

      ////////// RETURN SIGNALS //////////
   }
}