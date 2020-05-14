/*

***********************************************************************************
* Copyright (C) 2016 - , BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef CHAT_WIDGET_H
#define CHAT_WIDGET_H

#include <memory>
#include <QPointer>
#include <QWidget>

#include "ChatWidgetStates/AbstractChatWidgetState.h"
#include "ChatProtocol/ChatClientService.h"
#include "ChatProtocol/ClientParty.h"
#include "OtcTypes.h"

class QItemSelection;

class ApplicationSettings;
class ArmoryConnection;
class AssetManager;
class AuthAddressManager;
class ChatOTCHelper;
class ChatPartiesTreeModel;
class MDCallbacksQt;
class OTCRequestViewModel;
class OTCWindowsManager;
class WalletSignerContainer;

namespace Ui {
   class ChatWidget;
}

namespace bs {
   namespace network {
      enum class UserType : int;
   }
   namespace sync {
      class WalletsManager;
   }
   class UTXOReservationManager;
}

namespace Blocksettle {
   namespace Communication {
      namespace ProxyTerminalPb {
         class Response;
      }
   }
}

class ChatWidget final : public QWidget
{
   Q_OBJECT

public:
   explicit ChatWidget(QWidget* parent = nullptr);
   ~ChatWidget() override;

   void init(const std::shared_ptr<ConnectionManager>& connectionManager
      , bs::network::otc::Env env
      , const Chat::ChatClientServicePtr &
      , const std::shared_ptr<spdlog::logger> &
      , const std::shared_ptr<bs::sync::WalletsManager> &
      , const std::shared_ptr<AuthAddressManager> &
      , const std::shared_ptr<ArmoryConnection> &
      , const std::shared_ptr<WalletSignerContainer> &
      , const std::shared_ptr<MDCallbacksQt> &
      , const std::shared_ptr<AssetManager> &
      , const std::shared_ptr<bs::UTXOReservationManager> &
      , const std::shared_ptr<ApplicationSettings>&
   );

   bs::network::otc::Peer *currentPeer() const;

   void setUserType(bs::network::UserType userType);

protected:
   void showEvent(QShowEvent* e) override;
   void hideEvent(QHideEvent* event) override;
   bool eventFilter(QObject* sender, QEvent* event) override;

public slots:
   void onProcessOtcPbMessage(const Blocksettle::Communication::ProxyTerminalPb::Response &response) const;
   void onSendOtcMessage(const std::string& contactId, const BinaryData& data) const;
   void onSendOtcPublicMessage(const BinaryData& data) const;

   void onNewChatMessageTrayNotificationClicked(const QString& partyId);
   void onUpdateOTCShield() const;

   void onEmailHashReceived(const std::string &email, const std::string &hash) const;
   void onUserPublicKeyChanged(const Chat::UserPublicKeyInfoList& userPublicKeyInfoList);

private slots:
   void onPartyModelChanged() const;
   void onLogin();
   void onLogout();
   void onSendMessage() const;
   void onMessageRead(const std::string& partyId, const std::string& messageId) const;
   void onSendArrived(const Chat::MessagePtrList& messagePtrList) const;
   void onClientPartyStatusChanged(const Chat::ClientPartyPtr& clientPartyPtr) const;
   void onMessageStateChanged(const std::string& partyId, const std::string& message_id, int party_message_state) const;
   void onUserListClicked(const QModelIndex& index);
   void onActivatePartyId(const QString& partyId);
   void onActivateGlobalPartyId();
   void onActivateCurrentPartyId();
   void onActivateGlobalOTCTableRow() const;
   void onRegisterNewChangingRefresh();
   void onShowUserRoom(const QString& userHash);
   void onContactFriendRequest(const QString& userHash) const;
   void onSetDisplayName(const std::string& partyId, const std::string& contactName) const;
   void onConfirmContactNewKeyData(const Chat::UserPublicKeyInfoList& userPublicKeyInfoList, bool bForceUpdateAllUsers);
   void onPrivateMessagesHistoryCount(const std::string& partyId, quint64 count) const;

   void onOtcRequestCurrentChanged(const QModelIndex &current, const QModelIndex &previous) const;

   void onContactRequestAcceptClicked(const std::string& partyId) const;
   void onContactRequestRejectClicked(const std::string& partyId) const;
   void onContactRequestSendClicked(const std::string& partyId) const;
   void onContactRequestCancelClicked(const std::string& partyId) const;

   void onNewPartyRequest(const std::string& userName, const std::string& initialMessage) const;
   void onRemovePartyRequest(const std::string& partyId) const;

   void onOtcUpdated(const bs::network::otc::Peer *peer);
   void onOtcPublicUpdated() const;
   void onOTCPeerError(const bs::network::otc::Peer *peer, bs::network::otc::PeerErrorType type, const std::string* errorMsg);

   void onOtcRequestSubmit() const;
   void onOtcResponseAccept() const;
   void onOtcResponseUpdate() const;
   void onOtcQuoteRequestSubmit() const;
   void onOtcQuoteResponseSubmit() const;
   void onOtcPullOrRejectCurrent() const;

   void onOtcPrivatePartyReady(const Chat::ClientPartyPtr& clientPartyPtr) const;

   void onRequestAllPrivateMessages() const;

signals:
   // OTC
   void sendOtcPbMessage(const std::string& data);
   void chatRoomChanged();
   void requestPrimaryWalletCreation();
   void emailHashRequested(const std::string &email);
   void onAboutToHide();

private:
   friend class AbstractChatWidgetState;
   friend class ChatLogOutState;
   friend class IdleState;
   friend class PrivatePartyInitState;
   friend class PrivatePartyUninitState;
   friend class PrivatePartyRequestedOutgoingState;
   friend class PrivatePartyRequestedIncomingState;

   template <typename stateType, typename = typename std::enable_if<std::is_base_of<AbstractChatWidgetState, stateType>::value>::type>
      void changeState(std::function<void()>&& transitionChanges = []() {})
      {
         // Exit previous state
         stateCurrent_.reset();

         // Enter new state
         transitionChanges();
         stateCurrent_ = std::make_unique<stateType>(this);
         stateCurrent_->applyState();
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
   std::shared_ptr<OTCWindowsManager> otcWindowsManager_{};

   std::string ownUserId_;
   std::string currentPartyId_;
   QMap<std::string, QString> draftMessages_;
   bool bNeedRefresh_ = false;

   bs::network::UserType userType_{};
};

#endif // CHAT_WIDGET_H
