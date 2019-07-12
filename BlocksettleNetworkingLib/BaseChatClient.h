#ifndef __BASE_CHAT_CLIENT_H__
#define __BASE_CHAT_CLIENT_H__

#include "ZMQ_BIP15X_DataConnection.h"
#include "DataConnectionListener.h"
#include "ChatProtocol/ResponseHandler.h"
#include "Encryption/ChatSessionKey.h"
#include "Encryption/ContactPublicKey.h"
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

using UserHasherPtr = std::shared_ptr<UserHasher>;
using ChatDataPtr = std::shared_ptr<Chat::Data>;
using ChatDataVectorPtr = std::vector<ChatDataPtr>;
using UserNonceMap = std::map<std::string, Botan::SecureVector<uint8_t>>;
using MessagesQueue = std::queue<ChatDataPtr>;
using EnqueueMessagesMap = std::map<std::string, MessagesQueue>;
using PendingContactRequestMap = std::map<std::string, ChatDataPtr>;

class BaseChatClient : public QObject, public DataConnectionListener, public Chat::ResponseHandler
{
   Q_OBJECT

public:
   BaseChatClient(
      const std::shared_ptr<ConnectionManager>& /*connectionManager*/, 
      const std::shared_ptr<spdlog::logger>& /*logger*/,
      const QString& /*dbFile*/
   );

   ~BaseChatClient() noexcept override;

   BaseChatClient(const BaseChatClient&) = delete;
   BaseChatClient& operator = (const BaseChatClient&) = delete;

   BaseChatClient(BaseChatClient&&) = delete;
   BaseChatClient& operator = (BaseChatClient&&) = delete;

   std::string LoginToServer(
      const std::string& /*email*/, 
      const std::string& /*jwt*/,
      const ZmqBIP15XDataConnection::cbNewKey &/*callback*/
   );

   void LogoutFromServer();
   bool removeContactFromDB(const std::string &/*userId*/);

   void OnDataReceived(const std::string& /*data*/) override;
   void OnConnected() override;
   void OnDisconnected() override;
   void OnError(DataConnectionError) override;

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
   void OnAskForPublicKey(const Chat::Response_AskForPublicKey &/*response*/) override;

   // Called when we asked for a public key of peer, and got result.
   void OnSendOwnPublicKey(const Chat::Response_SendOwnPublicKey &/*response*/) override;
   void OnConfirmReplacePublicKey(const Chat::Response_ConfirmReplacePublicKey& /*response*/) override;

   void OnContactListConfirmed(const ChatDataVectorPtr& /*checked*/, const ChatDataVectorPtr& /*keyUpdate*/, const ChatDataVectorPtr& /*absolutelyNew*/);
   void OnContactListRejected(const ChatDataVectorPtr& /*rejectedList*/);
   void OnContactNewPublicKeyRejected(const std::string& /*userId*/);

   bool sendSearchUsersRequest(const std::string& /*userIdPattern*/);
   std::string deriveKey(const std::string& /*email*/) const;
   std::string getUserId() const;
   void uploadNewPublicKeyToServer(const bool& /*confirmed*/);

signals:
   void ConfirmContactsNewData(const ChatDataVectorPtr& /*remoteConfirmed*/, const ChatDataVectorPtr& /*remoteKeysUpdate*/, const ChatDataVectorPtr& /*remoteAbsolutelyNew*/);
   void ConfirmUploadNewPublicKey(const std::string&/*oldKey*/, const std::string&/*newKey*/);

protected:

   bool getContacts(ContactRecordDataList &/*contactList*/);
   bool addOrUpdateContact(const std::string &/*userId*/, Chat::ContactStatus /*status*/, const std::string &userName = "");
   bool encryptByIESAndSaveMessageInDb(const ChatDataPtr& /*message*/);

   ChatDataPtr encryptMessageToSendAEAD(const std::string& /*receiver*/, BinaryData& /*remotePublicKey*/, ChatDataPtr /*messageData*/);
   ChatDataPtr encryptMessageToSendIES(BinaryData& /*remotePublicKey*/, ChatDataPtr /*messageData*/);
   ChatDataPtr decryptIESMessage(const ChatDataPtr& /*message*/);

   void onFriendRequestReceived(const std::string& /*userId*/, const std::string& /*contactId*/, BinaryData /*publicKey*/, 
      const QDateTime& /*publicKeyTimestamp*/, const ChatDataPtr& /*message*/ = nullptr);
   void onFriendRequestAccepted(const std::string& /*contactId*/, BinaryData /*publicKey*/, const QDateTime& /*publicKeyTimestamp*/);
   void onFriendRequestRejected(const std::string& /*contactId*/);
   void onFriendRequestedRemove(const std::string& /*userId*/);

   void onServerApprovedFriendRemoving(const std::string& /*contactId*/);
   void cleanupConnection();
   void setSavedKeys(std::map<std::string, BinaryData>&& /*loadedKeys*/);

   virtual BinaryData         getOwnAuthPublicKey() const = 0;
   virtual SecureBinaryData   getOwnAuthPrivateKey() const = 0;
   virtual std::string        getChatServerHost() const = 0;
   virtual std::string        getChatServerPort() const = 0;
   virtual Chat::Data_Message_Encryption resolveMessageEncryption(ChatDataPtr /*message*/) const = 0;

   virtual void OnLoginCompleted() = 0;
   virtual void OnLogingFailed() = 0;
   virtual void OnLogoutCompleted() = 0;

   virtual void onRoomsLoaded(const ChatDataVectorPtr& /*roomsList*/) = 0;
   virtual void onUserListChanged(Chat::Command /*command*/, const std::vector<std::string>& /*userList*/) = 0;
   virtual void onContactListLoaded(const ChatDataVectorPtr& /*remoteContacts*/) = 0;
   virtual void onSearchResult(const ChatDataVectorPtr& /*userData*/) = 0;

   // either new message received or ours delivered
   virtual void onDMMessageReceived(const ChatDataPtr& /*messageData*/) = 0;
   virtual void onCRMessageReceived(const ChatDataPtr& /*messageData*/) = 0;
   virtual void onRoomMessageReceived(const ChatDataPtr& /*messageData*/) = 0;

   virtual void onMessageSent(const std::string& /*receiverId*/, const std::string& /*localId*/, const std::string& /*serverId*/) = 0;
   virtual void onMessageStatusChanged(const std::string& /*chatId*/, const std::string& /*messageId*/, int /*newStatus*/) = 0;

   virtual void onContactAccepted(const std::string& /*contactId*/) = 0;
   virtual void onContactRejected(const std::string& /*contactId*/) = 0;
   virtual void onFriendRequest(const std::string& /*userId*/, const std::string& /*contactId*/, const BinaryData& /*pk*/) = 0;
   virtual void onContactRemove(const std::string& /*contactId*/) = 0;
   virtual void onCreateOutgoingContact(const std::string& /*contactId*/);

   bool sendFriendRequestToServer(const std::string &/*friendUserId*/);
   bool sendFriendRequestToServer(const std::string &/*friendUserId*/, ChatDataPtr /*message*/, bool isFromPendings = false);
   bool sendAcceptFriendRequestToServer(const std::string &/*friendUserId*/);
   bool sendRejectFriendRequestToServer(const std::string &/*friendUserId*/);
   bool sendRemoveFriendToServer(const std::string& /*contactId*/);
   bool sendUpdateMessageState(const ChatDataPtr& /*message*/);

   ChatDataPtr sendMessageDataRequest(const ChatDataPtr& /*message*/, const std::string &/*receiver*/, bool isFromQueue = false);
   bool sendRequest(const Chat::Request& /*request*/);
   bool decodeAndUpdateIncomingSessionPublicKey(const std::string& /*senderId*/, const BinaryData& /*encodedPublicKey*/);
   void retrySendQueuedMessages(const std::string /*userId*/);
   void eraseQueuedMessages(const std::string /*userId*/);
   void retrySendQueuedContactRequests(const std::string &/*userId*/);
   void eraseQueuedContactRequests(const std::string& /*userId*/);

   std::shared_ptr<spdlog::logger>        logger_;
   std::unique_ptr<ChatDB>                chatDb_;
   std::string                            currentUserId_;

private:
   virtual void updateMessageStateAndSave(const ChatDataPtr& /*message*/, const Chat::Data_Message_State& /*state*/) = 0;
   void setInvalidAllMessagesForUser(const std::string& /*userId*/);

   std::shared_ptr<ConnectionManager>     connectionManager_;
   Chat::ContactPublicKeyPtr              contactPublicKeysPtr_;
   Chat::ChatSessionKeyPtr                chatSessionKeyPtr_;

   ZmqBIP15XDataConnectionPtr             connection_;
   UserHasherPtr                          hasher_;
   UserNonceMap                           userNonces_;
   // Queue of messages to be sent for each receiver, once we received the public key.
   EnqueueMessagesMap                     enqueuedMessages_;
   PendingContactRequestMap               pendingContactRequests_;

   std::string                            currentJwt_;
};

#endif // __BASE_CHAT_CLIENT_H__
