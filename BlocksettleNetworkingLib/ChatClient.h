#ifndef CHAT_CLIENT_H
#define CHAT_CLIENT_H

#include "BaseChatClient.h"
#include "ChatClientTree/TreeObjects.h"
#include "ChatHandleInterfaces.h"
#include "DataConnectionListener.h"

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

class ChatClient : public BaseChatClient
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

   void sendFriendRequest(const QString &friendUserId);
   void acceptFriendRequest(const QString &friendUserId);
   void declineFriendRequest(const QString &friendUserId);
   void clearSearch();
   bool isFriend(const QString &userId);

   Chat::ContactRecordData getContact(const QString &userId) const;

private:
   void readDatabase();

signals:
   void ConnectedToServer();

   void LoginFailed();
   void LoggedOut();
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

   void addMessageState(const std::shared_ptr<Chat::MessageData>& message, Chat::MessageData::State state);

protected:
   BinaryData getOwnAuthPublicKey() const override;
   SecureBinaryData   getOwnAuthPrivateKey() const override;
   std::string getChatServerHost() const override;
   std::string getChatServerPort() const override;

   void OnLoginCompleted() override;
   void OnLofingFailed() override;
   void OnLogoutCompleted() override;

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

   std::shared_ptr<ChatClientDataModel>   model_;
   std::shared_ptr<UserSearchModel>       userSearchModel_;
   std::shared_ptr<ChatTreeModelWrapper>  proxyModel_;

   bool              emailEntered_{ false };
};

#endif   // CHAT_CLIENT_H
