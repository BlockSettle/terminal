/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "ChatProtocol/ChatClientService.h"

using namespace Chat;

ChatClientService::ChatClientService(QObject* parent /*= nullptr*/)
   : ServiceThread<ChatClientLogic>(new ChatClientLogic, parent)
{
   qRegisterMetaType<CelerClient::CelerUserType>();
   qRegisterMetaType<Chat::LoggerPtr>();
   qRegisterMetaType<ZmqBipNewKeyCb>();
   qRegisterMetaType<Chat::ChatSettings>();

   ////////// PROXY SIGNALS //////////
   connect(this, &ChatClientService::Init, worker(), &ChatClientLogic::Init);
   connect(this, &ChatClientService::LoginToServer, worker(), &ChatClientLogic::LoginToServer);
   connect(this, &ChatClientService::LogoutFromServer, worker(), &ChatClientLogic::LogoutFromServer);
   connect(this, &ChatClientService::SendPartyMessage, worker(), &ChatClientLogic::SendPartyMessage);
   connect(this, &ChatClientService::RequestPrivateParty, worker(), &ChatClientLogic::RequestPrivateParty);
   connect(this, &ChatClientService::RequestPrivatePartyOTC, worker(), &ChatClientLogic::RequestPrivatePartyOTC);
   connect(this, &ChatClientService::SetMessageSeen, worker(), &ChatClientLogic::SetMessageSeen);
   connect(this, &ChatClientService::RejectPrivateParty, worker(), &ChatClientLogic::RejectPrivateParty);
   connect(this, &ChatClientService::DeletePrivateParty, worker(), &ChatClientLogic::DeletePrivateParty);
   connect(this, &ChatClientService::AcceptPrivateParty, worker(), &ChatClientLogic::AcceptPrivateParty);
   connect(this, &ChatClientService::SearchUser, worker(), &ChatClientLogic::SearchUser);
   connect(this, &ChatClientService::AcceptNewPublicKeys, worker(), &ChatClientLogic::AcceptNewPublicKeys);
   connect(this, &ChatClientService::DeclineNewPublicKeys, worker(), &ChatClientLogic::DeclineNewPublicKeys);

   ////////// RETURN SIGNALS //////////
   connect(worker(), &ChatClientLogic::chatUserUserHashChanged, this, &ChatClientService::chatUserUserHashChanged);
   connect(worker(), &ChatClientLogic::chatClientError, this, &ChatClientService::chatClientError);
   connect(worker(), &ChatClientLogic::clientLoggedOutFromServer, this, &ChatClientService::clientLoggedOutFromServer);
   connect(worker(), &ChatClientLogic::properlyConnected, this, &ChatClientService::clientLoggedInToServer);
   connect(worker(), &ChatClientLogic::partyModelChanged, this, &ChatClientService::partyModelChanged);
   connect(worker(), &ChatClientLogic::initDone, this, &ChatClientService::initDone);
   connect(worker(), &ChatClientLogic::searchUserReply, this, &ChatClientService::searchUserReply);
}

ClientPartyModelPtr ChatClientService::getClientPartyModelPtr() const
{
   return worker()->clientPartyModelPtr();
}
