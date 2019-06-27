#ifndef __BASE_CHAT_CLIENT_H__
#define __BASE_CHAT_CLIENT_H__

#include "ZMQ_BIP15X_DataConnection.h"
#include "DataConnectionListener.h"
#include "ChatProtocol/ResponseHandler.h"
#include "Encryption/ChatSessionKey.h"
#include "SecureBinaryData.h"
#include "ChatCommonTypes.h"
#include "ChatDB.h"

#include <botan/secmem.h>
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
   ~BaseChatClient() noexcept override;

   BaseChatClient(const BaseChatClient&) = delete;
   BaseChatClient& operator = (const BaseChatClient&) = delete;

   BaseChatClient(BaseChatClient&&) = delete;
   BaseChatClient& operator = (BaseChatClient&&) = delete;

   std::string LoginToServer(const std::string& email, const std::string& jwt
                             , const ZmqBIP15XDataConnection::cbNewKey &);
   void LogoutFromServer();

   bool removeContactFromDB(const std::string &userId);

public:
   void OnDataReceived(const std::string& data) override;
   void OnConnected() override;
   void OnDisconnected() override;
   void OnError(DataConnectionError errorCode) override;

public:
   void OnUsersList(const Chat::Response_UsersList &) override;
   void OnMessages(const Chat::Response_Messages &) override;
   void OnLoginReturned(const Chat::Response_Login &) override;
   void OnLogoutResponse(const Chat::Response_Logout &) override;
   void OnSendMessageResponse(const Chat::Response_SendMessage& ) override;
   void OnMessageChangeStatusResponse(const Chat::Response_MessageChangeStatus&) override;
   void OnModifyContactsDirectResponse(const Chat::Response_ModifyContactsDirect&) override;
   void OnModifyContactsServerResponse(const Chat::Response_ModifyContactsServer&) override;
   void OnContactsListResponse(const Chat::Response_ContactsList&) override;
   void OnChatroomsList(const Chat::Response_ChatroomsList&) override;
   void OnRoomMessages(const Chat::Response_RoomMessages&) override;
   void OnSearchUsersResponse(const Chat::Response_SearchUsers&) override;


   void OnSessionPublicKeyResponse(const Chat::Response_SessionPublicKey&) override;
   void OnReplySessionPublicKeyResponse(const Chat::Response_ReplySessionPublicKey&) override;
   // Called when a peer asks for our public key.
   void OnAskForPublicKey(const Chat::Response_AskForPublicKey &response) override;

   // Called when we asked for a public key of peer, and got result.
   void OnSendOwnPublicKey(const Chat::Response_SendOwnPublicKey &response) override;

protected:

   bool getContacts(ContactRecordDataList &contactList);
   bool addOrUpdateContact(const std::string &userId,
                           Chat::ContactStatus status,
                           const std::string &userName = "");

   bool encryptByIESAndSaveMessageInDb(const std::shared_ptr<Chat::Data>& message);
   std::shared_ptr<Chat::Data> encryptMessageToSendAEAD(const std::string& receiver,
                                                        BinaryData& remotePublicKey,
                                                        std::shared_ptr<Chat::Data> messageData);
   std::shared_ptr<Chat::Data> encryptMessageToSendIES(BinaryData& remotePublicKey,
                                                       std::shared_ptr<Chat::Data> messageData);
   std::shared_ptr<Chat::Data> decryptIESMessage(const std::shared_ptr<Chat::Data>& message);

   void onFriendRequestReceived(const std::string& userId, const std::string& contactId, BinaryData publicKey, const QDateTime& publicKeyTimestamp, const std::shared_ptr<Chat::Data>& message = nullptr);
   void onFriendRequestAccepted(const std::string& contactId, BinaryData publicKey, const QDateTime& publicKeyTimestamp);
   void onFriendRequestRejected(const std::string& contactId);
   void onFriendRequestedRemove(const std::string& userId);

   void onServerApprovedFriendRemoving(const std::string& contactId);

public:
   void OnContactListConfirmed(const std::vector<std::shared_ptr<Chat::Data>>& checked,
                               const std::vector<std::shared_ptr<Chat::Data>>& keyUpdate,
                               const std::vector<std::shared_ptr<Chat::Data>>& absolutelyNew);

public:
   bool sendSearchUsersRequest(const std::string& userIdPattern);
   std::string deriveKey(const std::string& email) const;

   std::string getUserId() const;

signals:
   void ConfirmContactsNewData(const std::vector<std::shared_ptr<Chat::Data>>& remoteConfirmed,
                               const std::vector<std::shared_ptr<Chat::Data>>& remoteKeysUpdate,
                               const std::vector<std::shared_ptr<Chat::Data>>& remoteAbsolutelyNew);

protected:
   void cleanupConnection();

   virtual BinaryData         getOwnAuthPublicKey() const = 0;
   virtual SecureBinaryData   getOwnAuthPrivateKey() const = 0;
   virtual std::string        getChatServerHost() const = 0;
   virtual std::string        getChatServerPort() const = 0;
   virtual Chat::Data_Message_Encryption resolveMessageEncryption(std::shared_ptr<Chat::Data> message) const = 0;

   void setSavedKeys(std::map<std::string, BinaryData>&& loadedKeys);

   virtual void OnLoginCompleted() = 0;
   virtual void OnLogingFailed() = 0;
   virtual void OnLogoutCompleted() = 0;

   virtual void onRoomsLoaded(const std::vector<std::shared_ptr<Chat::Data>>& roomsList) = 0;
   virtual void onUserListChanged(Chat::Command command, const std::vector<std::string>& userList) = 0;
   virtual void onContactListLoaded(const std::vector<std::shared_ptr<Chat::Data>>& remoteContacts) = 0;

   virtual void onSearchResult(const std::vector<std::shared_ptr<Chat::Data>>& userData) = 0;

   // either new message received or ours delivered
   virtual void onDMMessageReceived(const std::shared_ptr<Chat::Data>& messageData) = 0;
   virtual void onCRMessageReceived(const std::shared_ptr<Chat::Data>& messageData) = 0;
   virtual void onRoomMessageReceived(const std::shared_ptr<Chat::Data>& messageData) = 0;

   virtual void onMessageSent(const std::string& receiverId, const std::string& localId, const std::string& serverId) = 0;
   virtual void onMessageStatusChanged(const std::string& chatId, const std::string& messageId, int newStatus) = 0;

   virtual void onContactAccepted(const std::string& contactId) = 0;
   virtual void onContactRejected(const std::string& contactId) = 0;
   virtual void onFriendRequest(const std::string& userId, const std::string& contactId, const BinaryData& pk) = 0;
   virtual void onContactRemove(const std::string& contactId) = 0;
   virtual void onCreateOutgoingContact(const std::string& contactId);

protected:
   bool sendFriendRequestToServer(const std::string &friendUserId);
   bool sendFriendRequestToServer(const std::string &friendUserId, std::shared_ptr<Chat::Data> message, bool isFromPendings = false);
   bool sendAcceptFriendRequestToServer(const std::string &friendUserId);
   bool sendRejectFriendRequestToServer(const std::string &friendUserId);
   bool sendRemoveFriendToServer(const std::string& contactId);
   bool sendUpdateMessageState(const std::shared_ptr<Chat::Data>& message);

   std::shared_ptr<Chat::Data> sendMessageDataRequest(const std::shared_ptr<Chat::Data>& message
                                                      , const std::string &receiver, bool isFromQueue = false);

   bool sendRequest(const Chat::Request& request);

   bool decodeAndUpdateIncomingSessionPublicKey(const std::string& senderId, const BinaryData& encodedPublicKey);

   void retrySendQueuedMessages(const std::string userId);
   void eraseQueuedMessages(const std::string userId);

   void retrySendQueuedContactRequests(const std::string &userId);
   void eraseQueuedContactRequests(const std::string& userId);

protected:
   std::shared_ptr<spdlog::logger>        logger_;
   std::unique_ptr<ChatDB>                chatDb_;
   std::string                            currentUserId_;

private:
   std::shared_ptr<ConnectionManager>     connectionManager_;

   std::map<std::string, BinaryData>                  contactPublicKeys_;
   Chat::ChatSessionKeyPtr                            chatSessionKeyPtr_;
   std::shared_ptr<ZmqBIP15XDataConnection>           connection_;
   std::shared_ptr<UserHasher>                        hasher_;
   std::map<std::string, Botan::SecureVector<uint8_t>>    userNonces_;
   // Queue of messages to be sent for each receiver, once we received the public key.
   using messages_queue = std::queue<std::shared_ptr<Chat::Data> >;
   std::map<std::string, messages_queue>    enqueued_messages_;
   std::map<std::string, std::shared_ptr<Chat::Data>>    pending_contact_requests_;

   std::string       currentJwt_;
};
#endif // __BASE_CHAT_CLIENT_H__
