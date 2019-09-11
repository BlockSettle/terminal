#ifndef CHAT_WIDGET_H
#define CHAT_WIDGET_H

#include <memory>
#include <QPointer>
#include <QWidget>
#include "ChatProtocol/ChatClientService.h"
#include "ChatProtocol/ClientParty.h"

class AbstractChatWidgetState;
class ArmoryConnection;
class ChatPartiesTreeModel;
class OTCRequestViewModel;
class SignContainer;
class WalletsM;
class ChatOTCHelper;

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

   void init(const std::shared_ptr<ConnectionManager>& connectionManager
      , const std::shared_ptr<ApplicationSettings>& appSettings
      , const Chat::ChatClientServicePtr& chatClientServicePtr
      , const std::shared_ptr<spdlog::logger>& loggerPtr
      , const std::shared_ptr<bs::sync::WalletsManager> &walletsMgr
      , const std::shared_ptr<ArmoryConnection> &armory
      , const std::shared_ptr<SignContainer> &signContainer);

   std::string login(const std::string& email, const std::string& jwt, const ZmqBipNewKeyCb&);

protected:
   void showEvent(QShowEvent* e) override;
   bool eventFilter(QObject* sender, QEvent* event) override;

public slots:
   void onProcessOtcPbMessage(const std::string& data);
   void onSendOtcMessage(const std::string& partyId, const BinaryData& data);

   void onNewChatMessageTrayNotificationClicked(const QString& partyId);

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
   void onActivatePartyId(const QString& partyId);
   void onRegisterNewChangingRefresh();
   void onShowUserRoom(const QString& userHash);
   void onContactFriendRequest(const QString& userHash);
   void onSetDisplayName(const std::string& partyId, const std::string& contactName);

   void onContactRequestAcceptClicked(const std::string& partyId);
   void onContactRequestRejectClicked(const std::string& partyId);
   void onContactRequestSendClicked(const std::string& partyId);
   void onContactRequestCancelClicked(const std::string& partyId);

   void onNewPartyRequest(const std::string& userName);
   void onRemovePartyRequest(const std::string& partyId);

   void onOtcUpdated(const std::string& partyId);

   void onOtcRequestSubmit();
   void onOtcRequestPull();
   void onOtcResponseAccept();
   void onOtcResponseUpdate();
   void onOtcResponseReject();

signals:
   // OTC
   void sendOtcPbMessage(const std::string& data);

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
   QPointer<ChatOTCHelper> otcHelper_{};
   std::shared_ptr<spdlog::logger>  loggerPtr_;
   std::shared_ptr<ChatPartiesTreeModel> chatPartiesTreeModel_;

   std::string ownUserId_;
   std::string  currentPartyId_;
   QMap<std::string, QString> draftMessages_;
   bool bNeedRefresh_ = false;
};

#endif // CHAT_WIDGET_H
