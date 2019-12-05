/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef CHATCLIENTSERVICE_H
#define CHATCLIENTSERVICE_H

#include <memory>

#include <QObject>

#include "CelerClient.h"
#include "ChatProtocol/ServiceThread.h"
#include "ChatProtocol/ChatClientLogic.h"

namespace Chat
{
   class ChatClientLogic;

   class ChatClientService : public ServiceThread<ChatClientLogic>
   {
      Q_OBJECT

   public:
      explicit ChatClientService(QObject* parent = nullptr);

      ClientPartyModelPtr getClientPartyModelPtr() const;

   signals:
      ////////// PROXY SIGNALS //////////
      void Init(const Chat::LoggerPtr& loggerPtr, Chat::ChatSettings);
      void LoginToServer(const BinaryData& token, const BinaryData& tokenSign, const ZmqBipNewKeyCb& cb);
      void LogoutFromServer();
      void SendPartyMessage(const std::string& partyId, const std::string& data);
      void RequestPrivateParty(const std::string& userName, const std::string& initialMessage = "");
      void RequestPrivatePartyOTC(const std::string& remoteUserName);
      void SetMessageSeen(const std::string& partyId, const std::string& messageId);
      void RejectPrivateParty(const std::string& partyId);
      void DeletePrivateParty(const std::string& partyId);
      void AcceptPrivateParty(const std::string& partyId);
      void SearchUser(const std::string& userHash, const std::string& searchId);
      void AcceptNewPublicKeys(const Chat::UserPublicKeyInfoList& userPublicKeyInfoList);
      void DeclineNewPublicKeys(const Chat::UserPublicKeyInfoList& userPublicKeyInfoList);

      ////////// RETURN SIGNALS //////////
      void chatUserUserHashChanged(const std::string& chatUserUserHash);
      void chatClientError(const Chat::ChatClientLogicError& errorCode);
      void clientLoggedOutFromServer();
      void clientLoggedInToServer();
      void partyModelChanged();
      void initDone();
      void searchUserReply(const Chat::SearchUserReplyList& userHashList, const std::string& searchId);
   };

   using ChatClientServicePtr = std::shared_ptr<ChatClientService>;

}

Q_DECLARE_METATYPE(CelerClient::CelerUserType)
Q_DECLARE_METATYPE(Chat::LoggerPtr)
Q_DECLARE_METATYPE(ZmqBipNewKeyCb)

#endif // CHATCLIENTSERVICE_H
