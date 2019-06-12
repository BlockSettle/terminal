#ifndef ResponseHandler_h__
#define ResponseHandler_h__

namespace Chat {
   class AskForPublicKeyResponse;
   class ChatroomsListResponse;
   class ContactsActionResponseDirect;
   class ContactsActionResponseServer;
   class ContactsListResponse;
   class HeartbeatPongResponse;
   class LoginResponse;
   class LogoutResponse;
   class MessageChangeStatusResponse;
   class MessagesResponse;
   class ReplySessionPublicKeyResponse;
   class RoomMessagesResponse;
   class SearchUsersResponse;
   class SendMessageResponse;
   class SendOwnPublicKeyResponse;
   class SendRoomMessageResponse;
   class SessionPublicKeyResponse;
   class UsersListResponse;

   class ResponseHandler
   {
   public:
      virtual ~ResponseHandler() = default;
      virtual void OnUsersList(const UsersListResponse &) = 0;
      virtual void OnMessages(const MessagesResponse &) = 0;

      // Received a call from a peer to send our public key.
      virtual void OnAskForPublicKey(const AskForPublicKeyResponse &) = 0;

      // Received public key of one of our peers.
      virtual void OnSendOwnPublicKey(const SendOwnPublicKeyResponse &) = 0;

      virtual void OnLoginReturned(const LoginResponse &) = 0;
      virtual void OnLogoutResponse(const LogoutResponse &) = 0;

      virtual void OnSendMessageResponse(const SendMessageResponse&) = 0;
      virtual void OnMessageChangeStatusResponse(const MessageChangeStatusResponse&) = 0;
      virtual void OnContactsActionResponseDirect(const ContactsActionResponseDirect&) = 0;
      virtual void OnContactsActionResponseServer(const ContactsActionResponseServer&) = 0;
      virtual void OnContactsListResponse(const ContactsListResponse&) = 0;

      virtual void OnChatroomsList(const ChatroomsListResponse&) = 0;
      virtual void OnRoomMessages(const RoomMessagesResponse&) = 0;

      virtual void OnSearchUsersResponse(const SearchUsersResponse&) = 0;

      virtual void OnSessionPublicKeyResponse(const SessionPublicKeyResponse&) = 0;
      virtual void OnReplySessionPublicKeyResponse(const ReplySessionPublicKeyResponse&) = 0;
   };
}

#endif // ResponseHandler_h__
