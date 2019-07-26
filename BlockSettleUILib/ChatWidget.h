#ifndef CHAT_WIDGET_H
#define CHAT_WIDGET_H

#include <QItemSelection>
#include <QLayoutItem>
#include <QScopedPointer>
#include <QStringListModel>
#include <QWidget>

#include "ChatHandleInterfaces.h"
#include "CommonTypes.h"
#include "ZMQ_BIP15X_Helpers.h"
#include <memory>

namespace Ui {
   class ChatWidget;
}
namespace spdlog {
   class logger;
}

namespace Chat {
   class RoomData;
   class UserData;
   class OTCResponseData;
   class OTCRequestData;
}

class ChatClient;
class ConnectionManager;
class ApplicationSettings;
class ChatWidgetState;
class OTCRequestViewModel;
class ChatTreeModelWrapper;
class BaseCelerClient;
class QTextEdit;
class ChatWidget : public QWidget
                 , public ViewItemWatcher
                 , public NewMessageMonitor
                 , public ChatItemActionsHandler
{
   Q_OBJECT

public:
   enum State {
      LoggedIn,
      LoggedOut
   };
   //friend class ChatWidgetState;
   friend class ChatWidgetStateLoggedOut;
   friend class ChatWidgetStateLoggedIn;

   explicit ChatWidget(QWidget *parent = nullptr);
   ~ChatWidget() override;

   void init(const std::shared_ptr<ConnectionManager>& connectionManager
           , const std::shared_ptr<ApplicationSettings> &appSettings
           , const std::shared_ptr<spdlog::logger>& logger);

   std::string login(const std::string& email, const std::string& jwt
      , const ZmqBipNewKeyCb &);
   void logout();
   bool hasUnreadMessages();
   void setCelerClient(std::shared_ptr<BaseCelerClient> celerClient);
   void updateChat(const bool &isChatTab);

   // Sends friend request to PB contact if needed
   // TODO: Add PB's public key verfication
   void connectToPb(const std::string &pbUserId);

public slots:
   void onLoggedOut();
   void onNewChatMessageTrayNotificationClicked(const QString &userId);

private slots:
   void onSendButtonClicked();
   void onMessagesUpdated();
   void onLoginFailed();
   void onUsersDeleted(const std::vector<std::string> &);
   void onSendFriendRequest(const QString &userId);
   void onAddChatRooms(const std::vector<std::shared_ptr<Chat::Data> >& roomList);
   void onConnectedToServer();
   void selectGlobalRoom();
   void onContactRequestAccepted(const std::string &userId);
   void onChangeChatRoom(const QString &userId);
   void onConfirmUploadNewPublicKey(const std::string &oldKey, const std::string &newKey);
   void onContactChanged();
   void onBSChatInputSelectionChanged();
   void onChatMessagesSelectionChanged();
   void onContactRequestAcceptSendClicked();
   void onContactRequestRejectCancelClicked();
   void onContactListConfirmationRequested(const std::vector<std::shared_ptr<Chat::Data>>& remoteConfirmed,
                                           const std::vector<std::shared_ptr<Chat::Data>>& remoteKeysUpdate,
                                           const std::vector<std::shared_ptr<Chat::Data>>& remoteAbsolutelyNew);

   void onDMMessageReceived(const std::shared_ptr<Chat::Data>& messageData);
   void onContactRequestApproved(const std::string &userId);

   // OTC UI slots
   void OnOTCRequestCreated();
   void OnCreateResponse();
   void OnCancelCurrentTrading();

   void OnUpdateTradeRequestor();
   void OnAcceptTradeRequestor();
   void OnUpdateTradeResponder();
   void OnAcceptTradeResponder();

   void OnOTCSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected);

signals:
   void LoginFailed();
   void LogOut();

private:
   void SetOTCLoggedInState();
   void SetLoggedOutOTCState();

   void OTCSwitchToCommonRoom();
   void OTCSwitchToGlobalRoom();
   void OTCSwitchToSupportRoom();
   void OTCSwitchToRoom(std::shared_ptr<Chat::Data>& room);
   void OTCSwitchToContact(std::shared_ptr<Chat::Data>& contact, bool onlineStatus);
   void OTCSwitchToResponse(std::shared_ptr<Chat::Data>& response);

   void onConfirmContactNewKeyData(const std::vector<std::shared_ptr<Chat::Data>>& remoteConfirmed,
      const std::vector<std::shared_ptr<Chat::Data>>& remoteKeysUpdate,
      const std::vector<std::shared_ptr<Chat::Data>>& remoteAbsolutelyNew,
      bool bForceUpdateAllUsers);

   // used to display proper widget if OTC room selected.
   // either create OTC or Pull OTC, if was submitted
   void DisplayCorrespondingOTCRequestWidget();

   bool IsOTCRequestSubmitted() const;
   bool IsOTCRequestAccepted() const;

   void DisplayCreateOTCWidget();
   void DisplayOwnSubmittedOTC();
   void DisplayOwnLiveOTC();

   bool IsOTCChatSelected() const;
   void UpdateOTCRoomWidgetIfRequired();

   bool TradingAvailableForUser() const;

   void clearCursorSelection(QTextEdit *element);

private:
   QScopedPointer<Ui::ChatWidget> ui_;

   std::shared_ptr<ChatClient>      client_;
   std::shared_ptr<spdlog::logger>  logger_;
   std::shared_ptr<BaseCelerClient>     celerClient_;

   std::string serverPublicKey_;
   std::string  currentChat_;
   bool isRoom_;
   bool isContactRequest_;
   QSpacerItem *chatUsersVerticalSpacer_;
   bool isChatTab_;

private:
   std::shared_ptr<ChatWidgetState> stateCurrent_;
   QMap<std::string, std::string> draftMessages_;
   bool needsToStartFirstRoom_;

private:
   OTCRequestViewModel *otcRequestViewModel_ = nullptr;
   int64_t chatLoggedInTimestampUtcInMillis_;
   std::vector<QVariantList> oldMessages_;

   std::string pbUserId_;

private:
   bool isRoom();
   bool isContactRequest();
   void setIsContactRequest(bool);
   void setIsRoom(bool);
   void changeState(ChatWidget::State state);
   void initSearchWidget();
   bool isLoggedIn() const;
   void tryBecomeContactWithPb();

   bool eventFilter(QObject *sender, QEvent *event) override;

   // ViewItemWatcher interface
public:
   void onElementSelected(CategoryElement *element) override;
   void onMessageChanged(std::shared_ptr<Chat::Data> message) override;
   void onElementUpdated(CategoryElement *element) override;
   void onCurrentElementAboutToBeRemoved() override;

   // NewMessageMonitor interface
public:
   void onNewMessagesPresent(std::map<std::string, std::shared_ptr<Chat::Data>> newMessages) override;

   // ChatItemActionsHandler interface
public:
   void onActionCreatePendingOutgoing(const std::string &userId) override;
   void onActionRemoveFromContacts(std::shared_ptr<Chat::Data> crecord) override;
   void onActionAcceptContactRequest(std::shared_ptr<Chat::Data> crecord) override;
   void onActionRejectContactRequest(std::shared_ptr<Chat::Data> crecord) override;
   void onActionEditContactRequest(std::shared_ptr<Chat::Data> crecord) override;
   bool onActionIsFriend(const std::string &userId) override;
};
#endif // CHAT_WIDGET_H
