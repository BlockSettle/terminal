#ifndef CHAT_WIDGET_H
#define CHAT_WIDGET_H
/*
#include <QItemSelection>
#include <QLayoutItem>
#include <QScopedPointer>
#include <QStringListModel>
#include <QWidget>

#include "ChatProtocol/ChatClientService.h"
#include "ChatProtocol/ClientParty.h"
#include "ChatHandleInterfaces.h"
#include "CommonTypes.h"
#include "ZMQ_BIP15X_Helpers.h"
#include <memory>

namespace spdlog {
   class logger;
}

namespace Chat {
   class RoomData;
   class UserData;
   class OTCResponseData;
   class OTCRequestData;
}

class ApplicationSettings;
class BaseCelerClient;
class ChatClient;
class ChatTreeModelWrapper;
class ChatWidgetState;
class ConnectionManager;
class OtcClient;
class QTextEdit;

class PartyTreeItem;
class SignContainer;
class AbstractChatWidgetState;

namespace bs { namespace sync {
   class WalletsManager;
} }

class ChatWidget : public QWidget
                 , public ViewItemWatcher
                 , public NewMessageMonitor
                 , public ChatItemActionsHandler
{
   Q_OBJECT

public:
   explicit ChatWidget(QWidget *parent = nullptr);
   ~ChatWidget() override;

   void init(const std::shared_ptr<ConnectionManager>& connectionManager
           , const std::shared_ptr<ApplicationSettings> &appSettings
           , const Chat::ChatClientServicePtr& chatClientServicePtr
           , const std::shared_ptr<spdlog::logger>& logger
           , const std::shared_ptr<bs::sync::WalletsManager> &walletsMgr
           , const std::shared_ptr<ArmoryConnection> &armory
           , const std::shared_ptr<SignContainer> &signContainer);

   std::string login(const std::string& email, const std::string& jwt
      , const ZmqBipNewKeyCb &);
   void logout();
   bool hasUnreadMessages();
   void setCelerClient(std::shared_ptr<BaseCelerClient> celerClient);
   void updateChat(const bool &isChatTab);

   // ViewItemWatcher interface
   void onElementSelected(CategoryElement *element) override {};
   void onMessageChanged(std::shared_ptr<Chat::Data> message) override;
   void onElementUpdated(CategoryElement* element) override;
   void onCurrentElementAboutToBeRemoved() override;

   // NewMessageMonitor interface
   void onNewMessagesPresent(std::map<std::string, std::shared_ptr<Chat::Data>> newMessages) override;

   // ChatItemActionsHandler interface
   void onActionCreatePendingOutgoing(const std::string& userId) override;
   void onActionRemoveFromContacts(std::shared_ptr<Chat::Data> crecord) override;
   void onActionAcceptContactRequest(std::shared_ptr<Chat::Data> crecord) override;
   void onActionRejectContactRequest(std::shared_ptr<Chat::Data> crecord) override;
   void onActionEditContactRequest(std::shared_ptr<Chat::Data> crecord) override;
   bool onActionIsFriend(const std::string& userId) override;

signals:
   void LoginFailed();
   void LogOut();

public slots:
   void onLoggedOut();
   void onNewChatMessageTrayNotificationClicked(const QString& userId);

   void processOtcPbMessage(const std::string &data);

private slots:
   void onSendButtonClicked();
   void onMessagesUpdated();
   void onLoginFailed();
   void onUsersDeleted(const std::vector<std::string>&);
   void onSendFriendRequest(const QString& userId);
   void onAddChatRooms(const std::vector<std::shared_ptr<Chat::Data> >& roomList);
   void onConnectedToServer();
   void selectGlobalRoom();
   void onContactRequestAccepted(const std::string& userId);
   void onConfirmUploadNewPublicKey(const std::string& oldKey, const std::string& newKey);
   void onContactChanged();
   void onBSChatInputSelectionChanged();
   void onChatMessagesSelectionChanged();
   void onContactRequestAcceptSendClicked();
   void onContactRequestRejectCancelClicked();
   void onContactListConfirmationRequested(const std::vector<std::shared_ptr<Chat::Data>>& remoteConfirmed,
      const std::vector<std::shared_ptr<Chat::Data>>& remoteKeysUpdate,
      const std::vector<std::shared_ptr<Chat::Data>>& remoteAbsolutelyNew);

   void onDMMessageReceived(const std::shared_ptr<Chat::Data>& messageData);
   void onContactRequestApproved(const std::string& userId);

   void OnOTCSelectionChanged(const QItemSelection& selected, const QItemSelection& deselected);

   void onOtcRequestSubmit();
   void onOtcRequestPull();
   void onOtcResponseAccept();
   void onOtcResponseUpdate();
   void onOtcResponseReject();

   void onOtcUpdated(const std::string& contactId);

signals:
   void sendOtcPbMessage(const std::string &data);

private:
   void SetOTCLoggedInState();
   void SetLoggedOutOTCState();

   void OTCSwitchToCommonRoom();
   void OTCSwitchToGlobalRoom();
   void OTCSwitchToSupportRoom();
   void OTCSwitchToRoom(std::shared_ptr<Chat::Data>& room);
   void OTCSwitchToContact(std::shared_ptr<Chat::Data>& contact);
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

   void updateOtc(const std::string &contactId);

   bool isRoom();
   bool isContactRequest();
   void setIsContactRequest(bool);
   void setIsRoom(bool);
   //void changeState(ChatWidget::State state);
   void initSearchWidget();
   bool isLoggedIn() const;

   bool eventFilter(QObject* sender, QEvent* event) override;

   QScopedPointer<Ui::ChatWidget> ui_;

   std::shared_ptr<ChatClient>      client_;
   std::shared_ptr<spdlog::logger>  logger_;
   std::shared_ptr<BaseCelerClient> celerClient_;
   Chat::ChatClientServicePtr    chatClientServicePtr_;

   std::string serverPublicKey_;
   std::string  currentChat_;
   bool isRoom_;
   bool isContactRequest_;
   QSpacerItem *chatUsersVerticalSpacer_;
   bool isChatTab_;

   
   
   bool needsToStartFirstRoom_;

   OTCRequestViewModel *otcRequestViewModel_ = nullptr;
   int64_t chatLoggedInTimestampUtcInMillis_;
   std::vector<QVariantList> oldMessages_;

   OtcClient *otcClient_{};

   // #new_logic
private:
   friend class AbstractChatWidgetState;
   friend class ChatLogOutState;
   friend class IdleState;
   friend class PrivatePartyInitState;
   friend class PrivatePartyUninitState;
   friend class PrivatePartyRequestedOutgoingState;
   friend class PrivatePartyRequestedIncomingState;
   friend class PrivatePartyRejectedState;

   template <typename stateType
      , typename = std::enable_if<std::is_base_of<AbstractChatWidgetState, stateType>::value>::type>
   void changeState(std::function<void(void)>&& transitionChanges = [](){}) {
      // Exit previous state
      stateCurrent_.reset();

      // Enter new state
      transitionChanges();
      stateCurrent_ = std::make_unique<stateType>(this);
   }
protected:
   std::unique_ptr<AbstractChatWidgetState> stateCurrent_;
   std::shared_ptr<ChatPartiesTreeModel> chatPartiesTreeModel_;

   QMap<std::string, QString> draftMessages_;
public:
   //void onElementSelected(const PartyTreeItem* chatUserListElement);

private slots:
   // Users actions point
   void onUserListClicked(const QModelIndex& index);
   void onSendMessage();
   void onMessageRead(const std::string& partyId, const std::string& messageId);

   // Back end actions point
   void onLogin();
   void onLogout();
   void onSendArrived(const Chat::MessagePtrList& messagePtr);
   void onClientPartyStatusChanged(const Chat::ClientPartyPtr& clientPartyPtr);
   void onPartyModelChanged();
   void onMessageStateChanged(const std::string& partyId, const std::string& message_id, const int party_message_state);
};
*/

#include <memory>

#include <QWidget>

#include "ChatProtocol/ChatClientService.h"
#include "ChatProtocol/ClientParty.h"

class ArmoryConnection;
class SignContainer;
class ChatPartiesTreeModel;
class OTCRequestViewModel;
class AbstractChatWidgetState;

namespace Ui {
   class ChatWidget;
}

namespace bs {
   namespace sync {
      class WalletsManager;
   }
}

class ChatWidget : public QWidget
{
   Q_OBJECT

public:
   explicit ChatWidget(QWidget* parent = nullptr);
   ~ChatWidget() override;

   void init(const std::shared_ptr<ConnectionManager>& connectionManager,
      const std::shared_ptr<ApplicationSettings>& appSettings,
      const Chat::ChatClientServicePtr& chatClientServicePtr,
      const std::shared_ptr<spdlog::logger>& loggerPtr);

   std::string login(const std::string& email, const std::string& jwt, const ZmqBipNewKeyCb&);
//   void logout();

protected:
   virtual void showEvent(QShowEvent* e);

public slots:
   // OTC
   void processOtcPbMessage(const std::string& data);
   void onNewChatMessageTrayNotificationClicked(const QString& userId);

private slots:
   void onPartyModelChanged();
   void onLogin();
   void onLogout();
   void onSendMessage();
   void onMessageRead(const std::string& partyId, const std::string& messageId);
   void onSendArrived(const Chat::MessagePtrList& messagePtr);
   void onClientPartyStatusChanged(const Chat::ClientPartyPtr& clientPartyPtr);
   void onMessageStateChanged(const std::string& partyId, const std::string& message_id, const int party_message_state);
   void onUserListClicked(const QModelIndex& index);
   void onActivatePartyId(const QString& userId);
   void onRegisterNewChangingRefresh();
   void onShowUserRoom(const QString& userHash);
   void onContactFriendRequest(const QString& userHash);


   void onContactRequestAcceptClicked(const std::string& partyId);
   void onContactRequestRejectClicked(const std::string& partyId);
   void onContactRequestSendClicked(const std::string& partyId);
   void onContactRequestCancelClicked(const std::string& partyId);

   void onNewPartyRequest(const std::string& userName);
   void onRemovePartyRequest(const std::string& partyId);
signals:
   // OTC
   void sendOtcPbMessage(const std::string& data);

   // #new_logic
private:
   friend class AbstractChatWidgetState;
   friend class ChatLogOutState;
   friend class IdleState;
   friend class PrivatePartyInitState;
   friend class PrivatePartyUninitState;
   friend class PrivatePartyRequestedOutgoingState;
   friend class PrivatePartyRequestedIncomingState;
   friend class PrivatePartyRejectedState;

   template <typename stateType, typename = typename std::enable_if<std::is_base_of<AbstractChatWidgetState, stateType>::value>::type>
      void changeState(std::function<void(void)>&& transitionChanges = []() {})
      {
         // Exit previous state
         stateCurrent_.reset();

         // Enter new state
         transitionChanges();
         stateCurrent_ = std::make_unique<stateType>(this);
      }
protected:
   std::unique_ptr<AbstractChatWidgetState> stateCurrent_;

private:
   void chatTransition(const Chat::ClientPartyPtr& clientPartyPtr);
   QScopedPointer<Ui::ChatWidget> ui_;
   Chat::ChatClientServicePtr    chatClientServicePtr_;
   OTCRequestViewModel* otcRequestViewModel_ = nullptr;
   std::shared_ptr<spdlog::logger>  loggerPtr_;
   std::shared_ptr<ChatPartiesTreeModel> chatPartiesTreeModel_;

   std::string ownUserId_;
   std::string  currentPartyId_;
   QMap<std::string, QString> draftMessages_;

   bool bNeedRefresh_ = false;
};

#endif // CHAT_WIDGET_H
