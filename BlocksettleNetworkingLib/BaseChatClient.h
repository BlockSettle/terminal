#ifndef __BASE_CHAT_CLIENT_H__
#define __BASE_CHAT_CLIENT_H__

#include "ZMQ_BIP15X_DataConnection.h"
#include "DataConnectionListener.h"
#include "ChatProtocol/ResponseHandler.h"
#include "ChatProtocol/ChatProtocol.h"
#include "Encryption/ChatSessionKey.h"
#include "SecureBinaryData.h"
#include "ChatCommonTypes.h"
#include "ChatDB.h"

#include <spdlog/spdlog.h>

#include <queue>
#include <memory>
#include <map>

class ConnectionManager;
class UserHasher;

class BaseChatClient : public QObject
                     , public DataConnectionListener
                     , public Chat::ResponseHandler
{
   Q_OBJECT

public:
   BaseChatClient(const std::shared_ptr<ConnectionManager>& connectionManager
                  , const std::shared_ptr<spdlog::logger>& logger
                  , const QString& dbFile);
   ~BaseChatClient() noexcept;

   BaseChatClient(const BaseChatClient&) = delete;
   BaseChatClient& operator = (const BaseChatClient&) = delete;

   BaseChatClient(BaseChatClient&&) = delete;
   BaseChatClient& operator = (BaseChatClient&&) = delete;

   std::string LoginToServer(const std::string& email, const std::string& jwt
                             , const ZmqBIP15XDataConnection::cbNewKey &);
   void LogoutFromServer();

   void retrieveUserMessages(const QString &userId);
   void retrieveRoomMessages(const QString &roomId);

   bool removeContact(const QString &userId);

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

protected:

   bool getContacts(ContactRecordDataList &contactList);
   bool addOrUpdateContact(const QString &userId,
                           Chat::ContactStatus status,
                           const QString &userName = QStringLiteral(""));

   bool encryptByIESAndSaveMessageInDb(const std::shared_ptr<Chat::MessageData>& message);
   std::shared_ptr<Chat::MessageData> decryptIESMessage(const std::shared_ptr<Chat::MessageData>& message);

public:
   bool sendSearchUsersRequest(const QString& userIdPattern);
   QString deriveKey(const QString& email) const;

   QString getUserId() const;

private slots:
   void onCleanupConnection();

signals:
   void CleanupConnection();

protected:
   virtual BinaryData         getOwnAuthPublicKey() const = 0;
   virtual SecureBinaryData   getOwnAuthPrivateKey() const = 0;
   virtual std::string        getChatServerHost() const = 0;
   virtual std::string        getChatServerPort() const = 0;

   void setSavedKeys(std::map<QString, BinaryData>&& loadedKeys);

   virtual void OnLoginCompleted() = 0;
   virtual void OnLofingFailed() = 0;
   virtual void OnLogoutCompleted() = 0;

protected:
   bool sendFriendRequestToServer(const QString &friendUserId);
   bool sendAcceptFriendRequestToServer(const QString &friendUserId);
   bool sendDeclientFriendRequestToServer(const QString &friendUserId);
   bool sendUpdateMessageState(const std::shared_ptr<Chat::MessageData>& message);

   std::shared_ptr<Chat::MessageData> sendMessageDataRequest(const std::shared_ptr<Chat::MessageData>& message
                                                             , const QString &receiver);

   bool sendRequest(const std::shared_ptr<Chat::Request>& request);

   bool decodeAndUpdateIncomingSessionPublicKey(const std::string& senderId, const std::string& encodedPublicKey);

   void retrySendQueuedMessages(const std::string userId);
   void eraseQueuedMessages(const std::string userId);

protected:
   std::shared_ptr<spdlog::logger>        logger_;
   std::unique_ptr<ChatDB>                chatDb_;
   std::string                            currentUserId_;

private:
   std::shared_ptr<ConnectionManager>     connectionManager_;

   std::map<QString, BinaryData>                      contactPublicKeys_;
   Chat::ChatSessionKeyPtr                            chatSessionKeyPtr_;
   std::shared_ptr<ZmqBIP15XDataConnection>           connection_;
   std::shared_ptr<UserHasher>                        hasher_;
   std::map<QString, Botan::SecureVector<uint8_t>>    userNonces_;
   // Queue of messages to be sent for each receiver, once we received the public key.
   using messages_queue = std::queue<std::shared_ptr<Chat::MessageData> >;
   std::map<QString, messages_queue>    enqueued_messages_;

   std::string       currentJwt_;
};
#endif // __BASE_CHAT_CLIENT_H__
