/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
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

class AuthAddressManager;
class ArmoryConnection;
class ChatPartiesTreeModel;
class OTCRequestViewModel;
class SignContainer;
class WalletsM;
class ChatOTCHelper;
class OTCWindowsManager;
class MarketDataProvider;
class AssetManager;

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
}

namespace Blocksettle {
   namespace Communication {
      namespace ProxyTerminalPb {
         class Response;
      }
   }
}

class ChatWidget : public QWidget
{
   Q_OBJECT

public:
   explicit ChatWidget(QWidget* parent = nullptr);
   ~ChatWidget() override;

   void init(const std::shared_ptr<ConnectionManager>& connectionManager
      , bs::network::otc::Env env
      , const Chat::ChatClientServicePtr& chatClientServicePtr
      , const std::shared_ptr<spdlog::logger>& loggerPtr
      , const std::shared_ptr<bs::sync::WalletsManager> &walletsMgr
      , const std::shared_ptr<AuthAddressManager> &authManager
      , const std::shared_ptr<ArmoryConnection> &armory
      , const std::shared_ptr<SignContainer> &signContainer
      , const std::shared_ptr<MarketDataProvider>& mdProvider
      , const std::shared_ptr<AssetManager>& assetManager
   );

   bs::network::otc::Peer *currentPeer() const;

   void setUserType(bs::network::UserType userType);

protected:
   void showEvent(QShowEvent* e) override;
   bool eventFilter(QObject* sender, QEvent* event) override;

public slots:
   void onProcessOtcPbMessage(const Blocksettle::Communication::ProxyTerminalPb::Response &response);
   void onSendOtcMessage(const std::string& contactId, const BinaryData& data);
   void onSendOtcPublicMessage(const BinaryData& data);

   void onNewChatMessageTrayNotificationClicked(const QString& partyId);
   void onUpdateOTCShield();

   void onEmailHashReceived(const std::string &email, const std::string &hash);

private slots:
   void onPartyModelChanged();
   void onLogin();
   void onLogout();
   void onSendMessage();
   void onMessageRead(const std::string& partyId, const std::string& messageId);
   void onSendArrived(const Chat::MessagePtrList& messagePtrList);
   void onClientPartyStatusChanged(const Chat::ClientPartyPtr& clientPartyPtr);
   void onMessageStateChanged(const std::string& partyId, const std::string& message_id, const int party_message_state);
   void onUserListClicked(const QModelIndex& index);
   void onActivatePartyId(const QString& partyId);
   void onActivateGlobalPartyId();
   void onActivateCurrentPartyId();
   void onActivateGlobalOTCTableRow();
   void onRegisterNewChangingRefresh();
   void onShowUserRoom(const QString& userHash);
   void onContactFriendRequest(const QString& userHash);
   void onSetDisplayName(const std::string& partyId, const std::string& contactName);
   void onUserPublicKeyChanged(const Chat::UserPublicKeyInfoList& userPublicKeyInfoList);
   void onConfirmContactNewKeyData(const Chat::UserPublicKeyInfoList& userPublicKeyInfoList, bool bForceUpdateAllUsers);

   void onOtcRequestCurrentChanged(const QModelIndex &current, const QModelIndex &previous);

   void onContactRequestAcceptClicked(const std::string& partyId);
   void onContactRequestRejectClicked(const std::string& partyId);
   void onContactRequestSendClicked(const std::string& partyId);
   void onContactRequestCancelClicked(const std::string& partyId);

   void onNewPartyRequest(const std::string& userName, const std::string& initialMessage);
   void onRemovePartyRequest(const std::string& partyId);

   void onOtcUpdated(const bs::network::otc::Peer *peer);
   void onOtcPublicUpdated();
   void onOTCPeerError(const bs::network::otc::Peer *peer, bs::network::otc::PeerErrorType type, const std::string* errorMsg);

   void onOtcRequestSubmit();
   void onOtcResponseAccept();
   void onOtcResponseUpdate();
   void onOtcQuoteRequestSubmit();
   void onOtcQuoteResponseSubmit();
   void onOtcPullOrRejectCurrent();

   void onOtcPrivatePartyReady(const Chat::ClientPartyPtr& clientPartyPtr);

signals:
   // OTC
   void sendOtcPbMessage(const std::string& data);
   void chatRoomChanged();
   void requestPrimaryWalletCreation();
   void emailHashRequested(const std::string &email);

private:
   friend class AbstractChatWidgetState;
   friend class ChatLogOutState;
   friend class IdleState;
   friend class PrivatePartyInitState;
   friend class PrivatePartyUninitState;
   friend class PrivatePartyRequestedOutgoingState;
   friend class PrivatePartyRequestedIncomingState;

   template <typename stateType, typename = typename std::enable_if<std::is_base_of<AbstractChatWidgetState, stateType>::value>::type>
      void changeState(std::function<void(void)>&& transitionChanges = []() {})
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
