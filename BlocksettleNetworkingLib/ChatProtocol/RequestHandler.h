#ifndef REQUEST_HANDLER_H
#define REQUEST_HANDLER_H

#include <string>

namespace Chat {

   class Request_Login;
   class Request_Logout;
   class Request_SendMessage;
   class Request_AskForPublicKey;
   class Request_SendOwnPublicKey;
   class Request_OnlineUsers;
   class Request_Messages;
   class Request_MessageChangeStatus;
   class Request_ModifyContactsDirect;
   class Request_ModifyContactsServer;
   class Request_ContactsList;
   class Request_ChatroomsList;
   class Request_SendRoomMessage;
   class Request_SearchUsers;
   class Request_SessionPublicKey;
   class Request_ReplySessionPublicKey;
   class Request_UploadNewPublicKey;

   class RequestHandler
   {
   public:
      virtual ~RequestHandler() = default;

      virtual void OnLogin(const std::string& clientId, const Request_Login&) = 0;
      virtual void OnLogout(const std::string& clientId, const Request_Logout&) = 0;
      virtual void OnSendMessage(const std::string& clientId, const Request_SendMessage&) = 0;

      // Asking peer to send us their public key.
      virtual void OnAskForPublicKey(const std::string& clientId, const Request_AskForPublicKey&) = 0;

      // Sending our public key to the peer who asked for it.
      virtual void OnSendOwnPublicKey(const std::string& clientId, const Request_SendOwnPublicKey&) = 0;

      virtual void OnOnlineUsers(const std::string& clientId, const Request_OnlineUsers&) = 0;
      virtual void OnRequestMessages(const std::string& clientId, const Request_Messages&) = 0;

      virtual void OnRequestChangeMessageStatus(const std::string& clientId, const Request_MessageChangeStatus&) = 0;

      virtual void OnRequestContactsActionDirect(const std::string& clientId, const Request_ModifyContactsDirect&) = 0;
      virtual void OnRequestContactsActionServer(const std::string& clientId, const Request_ModifyContactsServer&) = 0;
      virtual void OnRequestContactsList(const std::string& clientId, const Request_ContactsList&) = 0;

      virtual void OnRequestChatroomsList(const std::string& clientId, const Request_ChatroomsList&) = 0;

      virtual void OnSendRoomMessage(const std::string& clientId, const Request_SendRoomMessage&) = 0;
      virtual void OnSearchUsersRequest(const std::string& clientId, const Request_SearchUsers&) = 0;

      virtual void OnSessionPublicKeyRequest(const std::string& clientId, const Request_SessionPublicKey&) = 0;
      virtual void OnReplySessionPublicKeyRequest(const std::string& clientId, const Request_ReplySessionPublicKey&) = 0;

      virtual void OnRequestUploadNewPublicKey(const Request_UploadNewPublicKey&) = 0;
   };
}

#endif // REQUEST_HANDLER_H
