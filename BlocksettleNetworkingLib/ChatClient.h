#ifndef CHAT_CLIENT_H
#define CHAT_CLIENT_H

#include "BaseChatClient.h"
#include "ChatClientTree/TreeObjects.h"
#include "ChatDB.h"
#include "ChatHandleInterfaces.h"
#include "ChatProtocol/ChatProtocol.h"
#include "ChatCommonTypes.h"
#include "DataConnectionListener.h"
#include "SecureBinaryData.h"
#include "Encryption/ChatSessionKey.h"

#include <queue>
#include <unordered_set>

#include <QAbstractItemModel>
#include <QObject>

namespace Chat {
   class Request;
}

class ApplicationSettings;
class ChatClientDataModel;
class UserHasher;
class UserSearchModel;
class ChatTreeModelWrapper;

class ChatClient : public QObject
             , public BaseChatClient
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

// DB related
   void retrieveUserMessages(const QString &userId);
   void retrieveRoomMessages(const QString &roomId);

   bool getContacts(ContactRecordDataList &contactList);
   bool addOrUpdateContact(const QString &userId,
                           Chat::ContactStatus status,
                           const QString &userName = QStringLiteral(""));
   bool removeContact(const QString &userId);
   Chat::ContactRecordData getContact(const QString &userId) const;

   bool encryptByIESAndSaveMessageInDb(const std::shared_ptr<Chat::MessageData>& message);


   bool sendFriendRequest(const QString &friendUserId);
   void acceptFriendRequest(const QString &friendUserId);
   void declineFriendRequest(const QString &friendUserId);
   void clearSearch();
   bool isFriend(const QString &userId);

   std::shared_ptr<Chat::MessageData> decryptIESMessage(const std::shared_ptr<Chat::MessageData>& message);

private:
   void readDatabase();

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
   void addMessageState(const std::shared_ptr<Chat::MessageData>& message, Chat::MessageData::State state);
   void retrySendQueuedMessages(const std::string userId);
   void eraseQueuedMessages(const std::string userId);

protected:
   BinaryData getOwnAuthPublicKey() const override;

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

private:
   std::shared_ptr<ApplicationSettings>   appSettings_;

   std::unique_ptr<ChatDB>                chatDb_;

   std::shared_ptr<ChatClientDataModel>   model_;
   std::shared_ptr<UserSearchModel>       userSearchModel_;
   std::shared_ptr<ChatTreeModelWrapper>  proxyModel_;

   bool              emailEntered_{ false };
};

#endif   // CHAT_CLIENT_H
