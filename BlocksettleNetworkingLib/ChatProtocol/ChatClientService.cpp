#include "ChatProtocol/ChatClientService.h"

namespace Chat
{

   ChatClientService::ChatClientService(QObject* parent /*= nullptr*/)
      : ServiceThread<ChatClientLogic>(new ChatClientLogic, parent)
   {
      qRegisterMetaType<Chat::ConnectionManagerPtr>();
      qRegisterMetaType<Chat::ApplicationSettingsPtr>();
      qRegisterMetaType<Chat::LoggerPtr>();
      qRegisterMetaType<ZmqBipNewKeyCb>();

      ////////// PROXY SIGNALS //////////
      connect(this, &ChatClientService::Init, worker(), &ChatClientLogic::Init);
      connect(this, &ChatClientService::LoginToServer, worker(), &ChatClientLogic::LoginToServer);
      connect(this, &ChatClientService::LogoutFromServer, worker(), &ChatClientLogic::LogoutFromServer);
      connect(this, &ChatClientService::SendPartyMessage, worker(), &ChatClientLogic::SendPartyMessage);

      ////////// RETURN SIGNALS //////////
      connect(worker(), &ChatClientLogic::chatUserDisplayNameChanged, this, &ChatClientService::chatUserDisplayNameChanged);
      connect(worker(), &ChatClientLogic::chatClientError, this, &ChatClientService::chatClientError);
      connect(worker(), &ChatClientLogic::clientLoggedOutFromServer, this, &ChatClientService::clientLoggedOutFromServer);
      connect(worker(), &ChatClientLogic::partyModelChanged, this, &ChatClientService::partyModelChanged);
   }

   ClientPartyModelPtr ChatClientService::getClientPartyModelPtr()
   {
      return worker()->clientPartyModelPtr();
   }

}