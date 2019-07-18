#ifndef CHAT_CLIENT_H
#define CHAT_CLIENT_H

#include "BaseChatClient.h"
#include "ChatClientTree/TreeObjects.h"
#include "ChatHandleInterfaces.h"
#include "DataConnectionListener.h"
#include "chat.pb.h"

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

   std::shared_ptr<Chat::Data> sendOwnMessage(
         const std::string& message, const std::string &receiver);
   std::shared_ptr<Chat::Data> SubmitPrivateOTCRequest(const bs::network::OTCRequest& otcRequest
                                                       , const std::string &receiver);
   std::shared_ptr<Chat::Data> SubmitPrivateOTCResponse(const bs::network::OTCResponse& otcResponse
                                                        , const std::string &receiver);
   std::shared_ptr<Chat::Data> SubmitPrivateCancel(const std::string &receiver);
   std::shared_ptr<Chat::Data> SubmitPrivateUpdate(const bs::network::OTCUpdate& update
                                                   , const std::string &receiver);

   std::shared_ptr<Chat::Data> sendRoomOwnMessage(
         const std::string& message, const std::string &receiver);

   void createPendingFriendRequest(const std::string& userId);
   void onContactRequestPositiveAction(const std::string& contactId, const std::string &message);
   void onContactRequestNegativeAction(const std::string& contactId);
   void sendFriendRequest(const std::string &friendUserId, const std::string& message = std::string());
   void acceptFriendRequest(const std::string &friendUserId);
   void rejectFriendRequest(const std::string &friendUserId);
   void removeFriendOrRequest(const std::string& userId);

   void clearSearch();
   bool isFriend(const std::string &userId);
   void onEditContactRequest(std::shared_ptr<Chat::Data> crecord);

   Chat::Data_ContactRecord getContact(const std::string &userId) const;

   void retrieveUserMessages(const std::string &userId);
   void loadRoomMessagesFromDB(const std::string& roomId);

private:
   void initMessage(Chat::Data *msg, const std::string& receiver);
   void updateMessageStateAndSave(const std::shared_ptr<Chat::Data>& message, const Chat::Data_Message_State& state);

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
   void MessageIdUpdated(const std::string& localId, const std::string& serverId, const std::string& chatId);
   void SearchUserListReceived(const std::vector<std::shared_ptr<Chat::Data>>& users, bool emailEntered);
   void NewContactRequest(const std::string &userId);
   void ContactRequestAccepted(const std::string &userId);
   void RoomsInserted();
   void ContactChanged();
   void DMMessageReceived(const std::shared_ptr<Chat::Data>& messageData);

protected:
   BinaryData getOwnAuthPublicKey() const override;
   SecureBinaryData   getOwnAuthPrivateKey() const override;
   std::string getChatServerHost() const override;
   std::string getChatServerPort() const override;
   Chat::Data_Message_Encryption resolveMessageEncryption(std::shared_ptr<Chat::Data> message) const override;

   void OnLoginCompleted() override;
   void OnLogingFailed() override;
   void OnLogoutCompleted() override;

   void onRoomsLoaded(const std::vector<std::shared_ptr<Chat::Data>>& roomsList) override;
   void onUserListChanged(Chat::Command command, const std::vector<std::string>& userList) override;
   void onContactListLoaded(const std::vector<std::shared_ptr<Chat::Data>>& remoteContacts) override;

   void onSearchResult(const std::vector<std::shared_ptr<Chat::Data>>& userData) override;

   void onDMMessageReceived(const std::shared_ptr<Chat::Data>& messageData) override;
   void onCRMessageReceived(const std::shared_ptr<Chat::Data>& messageData) override;
   void onRoomMessageReceived(const std::shared_ptr<Chat::Data>& messageData) override;

   void onMessageSent(const std::string& receiverId, const std::string& localId, const std::string& serverId) override;
   void onMessageStatusChanged(const std::string& chatId, const std::string& messageId, int newStatus) override;

   void onContactAccepted(const std::string& contactId) override;
   void onContactRejected(const std::string& contactId) override;
   void onFriendRequest(const std::string& userId, const std::string& contactId, const BinaryData& pk) override;
   void onContactRemove(const std::string& contactId) override;
   void onCreateOutgoingContact(const std::string& contactId) override;

   // ChatSearchActionsHandler interface
public:
   void onActionSearchUsers(const std::string &text) override;
   void onActionResetSearch() override;

   // ChatMessageReadHandler interface
public:
   void onMessageRead(std::shared_ptr<Chat::Data> message) override;
   void onRoomMessageRead(std::shared_ptr<Chat::Data> message) override;

   // ModelChangesHandler interface
public:
   void onContactUpdatedByInput(std::shared_ptr<Chat::Data> crecord) override;

private:
   std::shared_ptr<ApplicationSettings>   appSettings_;

   std::shared_ptr<ChatClientDataModel>   model_;
   std::shared_ptr<UserSearchModel>       userSearchModel_;
   std::shared_ptr<ChatTreeModelWrapper>  proxyModel_;

   bool              emailEntered_{ false };
};

#endif   // CHAT_CLIENT_H
