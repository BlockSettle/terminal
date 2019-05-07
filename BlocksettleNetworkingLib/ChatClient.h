#ifndef CHAT_CLIENT_H
#define CHAT_CLIENT_H


#include <QObject>
#include <QTimer>

#include "ChatProtocol/ChatProtocol.h"
#include "ChatDB.h"
#include "DataConnectionListener.h"
#include "SecureBinaryData.h"
#include <queue>
#include <QAbstractItemModel>

#include "ChatClientTree/TreeObjects.h"
#include "ChatHandleInterfaces.h"
namespace spdlog {
   class logger;
}
namespace Chat {
   class Request;
}


class ConnectionManager;
class ZmqBIP15XDataConnection;
class ApplicationSettings;
class UserHasher;
class ChatClientDataModel;


class ChatClient : public QObject
             , public DataConnectionListener
             , public Chat::ResponseHandler
             , public ChatItemActionsHandler
             , public ChatSearchActionsHandler
             , public ChatMessageReadHandler
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

   std::string loginToServer(const std::string& email, const std::string& jwt);
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

   void OnDataReceived(const std::string& data) override;
   void OnConnected() override;
   void OnDisconnected() override;
   void OnError(DataConnectionError errorCode) override;

   std::shared_ptr<Chat::MessageData> sendOwnMessage(
         const QString& message, const QString &receiver);
   std::shared_ptr<Chat::MessageData> sendRoomOwnMessage(
         const QString& message, const QString &receiver);

   void retrieveUserMessages(const QString &userId);
   void retrieveRoomMessages(const QString &roomId);

   // Called when a peer asks for our public key.
   void OnAskForPublicKey(const Chat::AskForPublicKeyResponse &response) override;

   // Called when we asked for a public key of peer, and got result.
   void OnSendOwnPublicKey(const Chat::SendOwnPublicKeyResponse &response) override;

   bool getContacts(ContactUserDataList &contactList);
   bool addOrUpdateContact(const QString &userId,
                           ContactUserData::Status status,
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
   QString getUserId();

private:
   void sendRequest(const std::shared_ptr<Chat::Request>& request);
   void readDatabase();

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
   void SearchUserListReceived(const std::vector<std::shared_ptr<Chat::UserData>>& users);

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



   std::unique_ptr<ChatDB>                   chatDb_;
   std::map<QString, autheid::PublicKey>     pubKeys_;
   std::shared_ptr<ZmqBIP15XDataConnection>  connection_;
   std::shared_ptr<UserHasher>               hasher_;
   std::map<QString, Botan::SecureVector<uint8_t>>   userNonces_;

   // Queue of messages to be sent for each receiver, once we received the public key.
   std::map<QString, std::queue<QString>>    enqueued_messages_;

   QTimer            heartbeatTimer_;

   std::string       currentUserId_;
   std::string       currentJwt_;
   std::atomic_bool  loggedIn_{ false };

   autheid::PrivateKey  ownPrivKey_;
   std::shared_ptr<ChatClientDataModel> model_;

   // ChatItemActionsHandler interface
public:
   void onActionAddToContacts(const QString& userId) override;
   void onActionRemoveFromContacts(std::shared_ptr<Chat::ContactRecordData> crecord) override;
   void onActionAcceptContactRequest(std::shared_ptr<Chat::ContactRecordData> crecord) override;
   void onActionRejectContactRequest(std::shared_ptr<Chat::ContactRecordData> crecord) override;

   // ChatSearchActionsHandler interface
public:
   void onActionSearchUsers(const std::string &text) override;
   void onActionResetSearch() override;

   // ChatMessageReadHandler interface
public:
   void onMessageRead(std::shared_ptr<Chat::MessageData> message) override;
};



#endif   // CHAT_CLIENT_H
