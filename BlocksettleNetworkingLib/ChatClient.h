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

   void OnDataReceived(const std::string& data) override;
   void OnConnected() override;
   void OnDisconnected() override;
   void OnError(DataConnectionError errorCode) override;

   /////////////////////////////////////////////////////////////////////////////
   // OTC related messages handling
   /////////////////////////////////////////////////////////////////////////////
   // HandleCommonOTCRequest - new OTC request in common OTC chat room
   void HandleCommonOTCRequest(const std::shared_ptr<Chat::OTCRequestData>& liveOTCRequest);

   // HandleCommonOTCRequestAccepted - our OTC request to common room was
   //    accepted by server
   void HandleCommonOTCRequestAccepted(const std::shared_ptr<Chat::OTCRequestData>& liveOTCRequest);

   // HandleCommonOTCRequestRejected - our OTC request to common room was
   //    rejected by server
   void HandleCommonOTCRequestRejected(const std::string& rejectReason);

   // HandleCommonOTCRequestCancelled - OTC request sent to common OTC room was
   //    cancelled by requestor. Could be both ours and someone else
   void HandleCommonOTCRequestCancelled(const QString& serverOTCId);

   void HandleCommonOTCRequestExpired(const QString& serverOTCId);

   // HandleAcceptedCommonOTCResponse - chat server accepts our response to
   //    OTC request in OTC chat room
   void HandleAcceptedCommonOTCResponse(const std::shared_ptr<Chat::OTCResponseData>& response);

   // HandleRejectedCommonOTCResponse - chat server accepts our response to
   //    OTC request in OTC chat room
   void HandleRejectedCommonOTCResponse(const QString& otcId, const std::string& reason);

   // HandleCommonOTCResponse - handle response we receive to our OTC request
   //    sent to common OTC chat room
   void HandleCommonOTCResponse(const std::shared_ptr<Chat::OTCResponseData>& response);

   // get update during negotiation
   void HandleOTCUpdate(const std::shared_ptr<Chat::OTCUpdateData>& update);

   /////////////////////////////////////////////////////////////////////////////

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
   bool encryptByIESAndSaveMessageInDb(const std::shared_ptr<Chat::MessageData>& message);
   bool decryptIESMessage(std::shared_ptr<Chat::MessageData>& message);
   QString getUserId();

public:
   // OTC related stubs
   // SubmitCommonOTCRequest - should send OTC request to chat server to OTC chat
   // Results:
   //    Can result in signals
   //       OTCRequestAccepted - request sent to OTC chat and was accepted
   //       OTCRequestRejected - OTC request was rejected by chat server.
   //    If return false - no signals will be emited
   // Returns:
   //    true - request was submitted
   //    false - request was not delivered to chat server.
   bool SubmitCommonOTCRequest(const bs::network::OTCRequest& request);

   // cancel current OTC request sent to OTC chat
   bool PullCommonOTCRequest(const QString& serverOTCId);

   bool SubmitCommonOTCResponse(const bs::network::OTCResponse& response);

private:
   // OTC related messaging endpoint
   bool sendCommonOTCRequest(const bs::network::OTCRequest& request);
   bool sendPullCommonOTCRequest();
   bool sendCommonOTCResponse();

   // OTC related signals
signals:
   // self OTC request accepted.
   void OTCRequestAccepted(const std::shared_ptr<Chat::OTCRequestData>& otcRequest);

   // self OTC request to OTC room was rejected by chat server
   void OTCOwnRequestRejected(const QString& reason);

   // we got a new OTC request from someone in OTC chat
   void NewOTCRequestReceived(const std::shared_ptr<Chat::OTCRequestData>& otcRequest);

   // OTC request was pulledby requestor. We should receive it even if it our own.
   // we could not just remove OTC, it should be initiated by chat server
   void OTCRequestCancelled(const QString& serverOTCId);

   // OTC request expired and is not settled
   void OTCRequestExpired(const QString& serverOTCId);

   // own OTC request sent to OTC chat expired
   void OwnOTCRequestExpired(const QString& serverOTCId);

   // CommonOTCResponseAccepted/CommonOTCResponseRejected - chat server accepted/rejected our
   //    response to OTC from common OTC chat
   void CommonOTCResponseAccepted(const std::shared_ptr<Chat::OTCResponseData>& otcResponse);
   void CommonOTCResponseRejected(const QString& serverOTCId, const QString& reason);

   // CommonOTCResponseReceived - we get response to our request sent to common
   //    OTC chat room
   void CommonOTCResponseReceived(const std::shared_ptr<Chat::OTCResponseData>& otcResponse);

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
   void NewContactRequest(const QString &userId);
   void ContactRequestAccepted(const QString &userId);

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
   std::shared_ptr<ChatTreeModelWrapper> proxyModel_;

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

private:
   /////////////////////////////////////////////////////////////////////////////
   // OTC simulation methods
   std::string GetNextRequestorId();
   std::string GetNextResponderId();
   std::string GetNextOTCId();
   std::string GetNextServerOTCId();
   std::string GetNextResponseId();
   std::string GetNextServerResponseId();
   std::string GetNextNegotiationChannelId();
   void ScheduleForExpire(const std::shared_ptr<Chat::OTCRequestData>& liveOTCRequest);
   /////////////////////////////////////////////////////////////////////////////

private:
   // OTC temp fields. will be removed after OTC goes through chat server
   uint64_t          nextOtcId_ = 1;
   const std::string baseFakeRequestorId_ = "fake_req";
   uint64_t          nextRequestorId_ = 1;
   const std::string baseFakeResponderId_ = "fake_resp";
   uint64_t          nextResponderId_ = 1;
   uint64_t          nextResponseId_ = 1;
   uint64_t          negotiationChannelId_ = 1;

   QString           ownSubmittedOTCId_;
   QString           ownServerOTCId_;

   // based on server reqest id
   std::unordered_set<std::string> aliveOtcRequests_;

   // ModelChangesHandler interface
public:
   void onContactUpdatedByInput(std::shared_ptr<Chat::ContactRecordData> crecord) override;
};
#endif   // CHAT_CLIENT_H
