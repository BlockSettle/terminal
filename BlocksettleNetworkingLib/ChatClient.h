#ifndef CHAT_CLIENT_H
#define CHAT_CLIENT_H

#include "ChatClientTree/TreeObjects.h"
#include "ChatDB.h"
#include "ChatHandleInterfaces.h"
#include "ChatProtocol/ChatProtocol.h"
#include "ChatCommonTypes.h"
#include "DataConnectionListener.h"
#include "SecureBinaryData.h"
#include "ZMQ_BIP15X_DataConnection.h"
#include "Encryption/ChatSessionKey.h"

#include <queue>
#include <unordered_set>

#include <QAbstractItemModel>
#include <QObject>
#include <QTimer>

namespace spdlog {
   class logger;
}
namespace Chat {
   class Request;
}

class ApplicationSettings;
class ChatClientDataModel;
class ConnectionManager;
class UserHasher;
class ZmqBIP15XDataConnection;
class UserSearchModel;
class ChatTreeModelWrapper;

class ChatClient : public QObject
             , public DataConnectionListener
             , public Chat::ResponseHandler
             , public ChatItemActionsHandler
             , public ChatSearchActionsHandler
             , public ChatMessageReadHandler
             , public ModelChangesHandler
{
   Q_OBJECT

public:
   ChatClient(const std::shared_ptr<ConnectionManager> &
            , const std::shared_ptr<ApplicationSettings> &
            , const std::shared_ptr<spdlog::logger> &);
   ~ChatClient() noexcept override;

   ChatClient(const ChatClient&) = delete;
   ChatClient& operator = (const ChatClient&) = delete;
   ChatClient(ChatClient&&) = delete;
   ChatClient& operator = (ChatClient&&) = delete;

   std::shared_ptr<ChatClientDataModel> getDataModel();
   std::shared_ptr<UserSearchModel> getUserSearchModel();
   std::shared_ptr<ChatTreeModelWrapper> getProxyModel();

   std::string loginToServer(const std::string& email, const std::string& jwt
      , const ZmqBIP15XDataConnection::cbNewKey &);
   void logout(bool send = true);

   void OnHeartbeatPong(const Chat::HeartbeatPongResponse &) override;
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

   void OnDataReceived(const std::string& data) override;
   void OnConnected() override;
   void OnDisconnected() override;
   void OnError(DataConnectionError errorCode) override;

   std::shared_ptr<Chat::MessageData> sendOwnMessage(
         const QString& message, const QString &receiver);
   std::shared_ptr<Chat::MessageData> SubmitPrivateOTCRequest(const bs::network::OTCRequest& otcRequest
                                                              , const QString &receiver);
   std::shared_ptr<Chat::MessageData> SubmitPrivateOTCResponse(const bs::network::OTCResponse& otcResponse
                                                              , const QString &receiver);
   std::shared_ptr<Chat::MessageData> SubmitPrivateCancel(const QString &receiver);
   std::shared_ptr<Chat::MessageData> SubmitPrivateUpdate(const bs::network::OTCUpdate& update
                                                          , const QString &receiver);

   std::shared_ptr<Chat::MessageData> sendRoomOwnMessage(
         const QString& message, const QString &receiver);

   void retrieveUserMessages(const QString &userId);
   void retrieveRoomMessages(const QString &roomId);

   // Called when a peer asks for our public key.
   void OnAskForPublicKey(const Chat::AskForPublicKeyResponse &response) override;

   // Called when we asked for a public key of peer, and got result.
   void OnSendOwnPublicKey(const Chat::SendOwnPublicKeyResponse &response) override;

   bool getContacts(ContactRecordDataList &contactList);
   bool addOrUpdateContact(const QString &userId,
                           Chat::ContactStatus status,
                           const QString &userName = QStringLiteral(""));
   bool removeContact(const QString &userId);
   void sendFriendRequest(const QString &friendUserId);
   void acceptFriendRequest(const QString &friendUserId);
   void declineFriendRequest(const QString &friendUserId);
   void sendUpdateMessageState(const std::shared_ptr<Chat::MessageData>& message);
   void sendSearchUsersRequest(const QString& userIdPattern);
   QString deriveKey(const QString& email) const;
   void clearSearch();
   bool isFriend(const QString &userId);
   Chat::ContactRecordData getContact(const QString &userId) const;
   bool encryptByIESAndSaveMessageInDb(const std::shared_ptr<Chat::MessageData>& message);
   std::shared_ptr<Chat::MessageData> decryptIESMessage(const std::shared_ptr<Chat::MessageData>& message);
   QString getUserId();

private:
   void sendRequest(const std::shared_ptr<Chat::Request>& request);
   void readDatabase();
   bool decodeAndUpdateIncomingSessionPublicKey(const std::string& senderId, const std::string& encodedPublicKey);

   std::shared_ptr<Chat::MessageData> sendMessageDataRequest(const std::shared_ptr<Chat::MessageData>& message
                                                             , const QString &receiver);

signals:
   void ConnectedToServer();
   void ConnectionClosed();
   void ConnectionError(int errorCode);

   void LoginFailed();
   void LoggedOut();
   void UsersReplace(const std::vector<std::string>& users);
   void UsersAdd(const std::vector<std::string>& users);
   void UsersDel(const std::vector<std::string>& users);
   void IncomingFriendRequest(const std::vector<std::string>& users);
   void FriendRequestAccepted(const std::vector<std::string>& users);
   void FriendRequestRejected(const std::vector<std::string>& users);
   void MessagesUpdate(const std::vector<std::shared_ptr<Chat::MessageData>> &messages, bool isFirstFetch);
   void RoomMessagesUpdate(const std::vector<std::shared_ptr<Chat::MessageData>> &messages, bool isFirstFetch);
   void MessageIdUpdated(const QString& localId, const QString& serverId,const QString& chatId);
   void MessageStatusUpdated(const QString& messageId, const QString& chatId, int newStatus);
   void RoomsAdd(const std::vector<std::shared_ptr<Chat::RoomData>>& rooms);
   void SearchUserListReceived(const std::vector<std::shared_ptr<Chat::UserData>>& users, bool emailEntered);
   void NewContactRequest(const QString &userId);
   void ContactRequestAccepted(const QString &userId);
   void RoomsInserted();

   void ForceLogoutSignal();
public slots:
   //void onMessageRead(const std::shared_ptr<Chat::MessageData>& message);

private slots:
   void onForceLogoutSignal();
   void sendHeartbeat();
   void addMessageState(const std::shared_ptr<Chat::MessageData>& message, Chat::MessageData::State state);
   void retrySendQueuedMessages(const std::string userId);
   void eraseQueuedMessages(const std::string userId);

private:
   std::shared_ptr<ConnectionManager>     connectionManager_;
   std::shared_ptr<ApplicationSettings>   appSettings_;
   std::shared_ptr<spdlog::logger>        logger_;

   std::unique_ptr<ChatDB>                                     chatDb_;
   std::map<QString, BinaryData>                               contactPublicKeys_;

   Chat::ChatSessionKeyPtr  chatSessionKeyPtr_;

   std::shared_ptr<ZmqBIP15XDataConnection>                    connection_;
   std::shared_ptr<UserHasher>                                 hasher_;
   std::map<QString, Botan::SecureVector<uint8_t>>             userNonces_;

   // Queue of messages to be sent for each receiver, once we received the public key.
   using messages_queue = std::queue<std::shared_ptr<Chat::MessageData> >;
   std::map<QString, messages_queue>    enqueued_messages_;

   QTimer            heartbeatTimer_;

   std::string       currentUserId_;
   std::string       currentJwt_;
   std::atomic_bool  loggedIn_{ false };

   autheid::PrivateKey  ownPrivKey_;
   std::shared_ptr<ChatClientDataModel> model_;
   std::shared_ptr<UserSearchModel> userSearchModel_;
   std::shared_ptr<ChatTreeModelWrapper> proxyModel_;

   bool              emailEntered_{ false };

   // ChatItemActionsHandler interface
public:
   void onActionAddToContacts(const QString& userId) override;
   void onActionRemoveFromContacts(std::shared_ptr<Chat::ContactRecordData> crecord) override;
   void onActionAcceptContactRequest(std::shared_ptr<Chat::ContactRecordData> crecord) override;
   void onActionRejectContactRequest(std::shared_ptr<Chat::ContactRecordData> crecord) override;
   bool onActionIsFriend(const QString& userId) override;

   // ChatSearchActionsHandler interface
public:
   void onActionSearchUsers(const std::string &text) override;
   void onActionResetSearch() override;

   // ChatMessageReadHandler interface
public:
   void onMessageRead(std::shared_ptr<Chat::MessageData> message) override;
   void onRoomMessageRead(std::shared_ptr<Chat::MessageData> message) override;

   // ModelChangesHandler interface
public:
   void onContactUpdatedByInput(std::shared_ptr<Chat::ContactRecordData> crecord) override;
};

#endif   // CHAT_CLIENT_H
