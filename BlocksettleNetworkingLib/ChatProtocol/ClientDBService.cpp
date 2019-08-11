#include "ChatProtocol/ClientDBService.h"

namespace Chat
{

   ClientDBService::ClientDBService(QObject* parent /* = nullptr */)
      : ServiceThread<ClientDBLogic>(new ClientDBLogic, parent)
   {
      qRegisterMetaType<Chat::ApplicationSettingsPtr>();

      ////////// PROXY SIGNALS //////////
      connect(this, &ClientDBService::Init, worker(), &ClientDBLogic::Init);
      connect(this, &ClientDBService::saveMessage, worker(), &ClientDBLogic::saveMessage);
      connect(this, &ClientDBService::updateMessageState, worker(), &ClientDBLogic::updateMessageState);

      ////////// RETURN SIGNALS //////////
      connect(worker(), &ClientDBLogic::initDone, this, &ClientDBService::initDone);
      connect(worker(), &ClientDBLogic::messageInserted, this, &ClientDBService::messageInserted);
      connect(worker(), &ClientDBLogic::messageStateChanged, this, &ClientDBService::messageStateChanged);
   }
}