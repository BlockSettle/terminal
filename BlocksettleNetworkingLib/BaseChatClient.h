#ifndef __BASE_CHAT_CLIENT_H__
#define __BASE_CHAT_CLIENT_H__

#include "ZMQ_BIP15X_DataConnection.h"
#include "DataConnectionListener.h"
#include "ChatProtocol/ResponseHandler.h"

#include <spdlog/spdlog.h>

class ConnectionManager;

class BaseChatClient : public DataConnectionListener
                     , public Chat::ResponseHandler
{
public:
   BaseChatClient(const std::shared_ptr<ConnectionManager>& connectionManager
                  , const std::shared_ptr<spdlog::logger>& logger);
   ~BaseChatClient() noexcept;

   BaseChatClient(const BaseChatClient&) = delete;
   BaseChatClient& operator = (const BaseChatClient&) = delete;

   BaseChatClient(BaseChatClient&&) = delete;
   BaseChatClient& operator = (BaseChatClient&&) = delete;

   std::string LoginToServer(const std::string& email, const std::string& jwt
                             , const ZmqBIP15XDataConnection::cbNewKey &);
   void LogoutFromServer();

public:
   void OnDataReceived(const std::string& data) override;
   void OnConnected() override;
   void OnDisconnected() override;
   void OnError(DataConnectionError errorCode) override;

public:
   void OnUsersList(const Chat::UsersListResponse &) override;
   void OnMessages(const Chat::MessagesResponse &) override;
   void OnLoginReturned(const Chat::LoginResponse &) override;
   void OnLogoutResponse(const Chat::LogoutResponse &) override;
   void OnSendMessageResponse(const Chat::SendMessageResponse& ) override;
   void OnMessageChangeStatusResponse(const Chat::MessageChangeStatusResponse&) override;
   void OnContactsActionResponseDirect(const Chat::ContactsActionResponseDirect&) override;
   void OnContactsActionResponseServer(const Chat::ContactsActionResponseServer&) override;
   void OnContactsListResponse(const Chat::ContactsListResponse&) override;
   void OnChatroomsList(const Chat::ChatroomsListResponse&) override;
   void OnRoomMessages(const Chat::RoomMessagesResponse&) override;
   void OnSearchUsersResponse(const Chat::SearchUsersResponse&) override;


   void OnSessionPublicKeyResponse(const Chat::SessionPublicKeyResponse&) override;
   void OnReplySessionPublicKeyResponse(const Chat::ReplySessionPublicKeyResponse&) override;
   // Called when a peer asks for our public key.
   void OnAskForPublicKey(const Chat::AskForPublicKeyResponse &response) override;

   // Called when we asked for a public key of peer, and got result.
   void OnSendOwnPublicKey(const Chat::SendOwnPublicKeyResponse &response) override;

public:
   bool sendSearchUsersRequest(const QString& userIdPattern);
   QString deriveKey(const QString& email) const;

   QString getUserId() const;

protected:
   virtual BinaryData getOwnAuthPublicKey() const = 0;

protected:
   bool sendFriendRequestToServer(const QString &friendUserId);
   bool sendAcceptFriendRequestToServer(const QString &friendUserId);
   bool sendDeclientFriendRequestToServer(const QString &friendUserId);
   bool sendUpdateMessageState(const std::shared_ptr<Chat::MessageData>& message);

private:
   bool sendRequest(const std::shared_ptr<Chat::Request>& request);

   bool decodeAndUpdateIncomingSessionPublicKey(const std::string& senderId, const std::string& encodedPublicKey);

protected:
   std::shared_ptr<spdlog::logger>        logger_;

private:
   std::shared_ptr<ConnectionManager>     connectionManager_;

   std::map<QString, BinaryData>                               contactPublicKeys_;
   Chat::ChatSessionKeyPtr  chatSessionKeyPtr_;
   std::shared_ptr<ZmqBIP15XDataConnection>                    connection_;
   std::shared_ptr<UserHasher>                                 hasher_;
   std::map<QString, Botan::SecureVector<uint8_t>>             userNonces_;
   // Queue of messages to be sent for each receiver, once we received the public key.
   using messages_queue = std::queue<std::shared_ptr<Chat::MessageData> >;
   std::map<QString, messages_queue>    enqueued_messages_;

   std::string       currentUserId_;
   std::string       currentJwt_;
};
#endif // __BASE_CHAT_CLIENT_H__
