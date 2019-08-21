#include "ChatProtocol/ClientDBService.h"

namespace Chat
{

   ClientDBService::ClientDBService(QObject* parent /* = nullptr */)
      : ServiceThread<ClientDBLogic>(new ClientDBLogic, parent)
   {
      qRegisterMetaType<Chat::ApplicationSettingsPtr>();
      qRegisterMetaType<Chat::CryptManagerPtr>();

      ////////// PROXY SIGNALS //////////
      connect(this, &ClientDBService::Init, worker(), &ClientDBLogic::Init);
      connect(this, &ClientDBService::saveMessage, worker(), &ClientDBLogic::saveMessage);
      connect(this, &ClientDBService::updateMessageState, worker(), &ClientDBLogic::updateMessageState);
      connect(this, &ClientDBService::createNewParty, worker(), &ClientDBLogic::createNewParty);
      connect(this, &ClientDBService::readUnsentMessages, worker(), &ClientDBLogic::readUnsentMessages);

      ////////// RETURN SIGNALS //////////
      connect(worker(), &ClientDBLogic::initDone, this, &ClientDBService::initDone);
      connect(worker(), &ClientDBLogic::messageArrived, this, &ClientDBService::messageArrived);
      connect(worker(), &ClientDBLogic::messageStateChanged, this, &ClientDBService::messageStateChanged);
      connect(worker(), &ClientDBLogic::messageLoaded, this, &ClientDBService::messageLoaded);
   }
}