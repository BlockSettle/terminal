#include "ChatProtocol/ChatClientService.h"

using namespace Chat;

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
   connect(this, &ChatClientService::RequestPrivateParty, worker(), &ChatClientLogic::RequestPrivateParty);
   connect(this, &ChatClientService::SetMessageSeen, worker(), &ChatClientLogic::SetMessageSeen);
   connect(this, &ChatClientService::RejectPrivateParty, worker(), &ChatClientLogic::RejectPrivateParty);
   connect(this, &ChatClientService::DeletePrivateParty, worker(), &ChatClientLogic::DeletePrivateParty);
   connect(this, &ChatClientService::AcceptPrivateParty, worker(), &ChatClientLogic::AcceptPrivateParty);
   connect(this, &ChatClientService::SearchUser, worker(), &ChatClientLogic::SearchUser);
   connect(this, &ChatClientService::AcceptNewPublicKeys, worker(), &ChatClientLogic::AcceptNewPublicKeys);
   connect(this, &ChatClientService::DeclineNewPublicKeys, worker(), &ChatClientLogic::DeclineNewPublicKeys);

   ////////// RETURN SIGNALS //////////
   connect(worker(), &ChatClientLogic::chatUserUserNameChanged, this, &ChatClientService::chatUserUserNameChanged);
   connect(worker(), &ChatClientLogic::chatClientError, this, &ChatClientService::chatClientError);
   connect(worker(), &ChatClientLogic::clientLoggedOutFromServer, this, &ChatClientService::clientLoggedOutFromServer);
   connect(worker(), &ChatClientLogic::properlyConnected, this, &ChatClientService::clientLoggedInToServer);
   connect(worker(), &ChatClientLogic::partyModelChanged, this, &ChatClientService::partyModelChanged);
   connect(worker(), &ChatClientLogic::initDone, this, &ChatClientService::initDone);
   connect(worker(), &ChatClientLogic::searchUserReply, this, &ChatClientService::searchUserReply);
}

ClientPartyModelPtr ChatClientService::getClientPartyModelPtr()
{
   return worker()->clientPartyModelPtr();
}
