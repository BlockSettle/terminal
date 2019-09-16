#ifndef __HEADLESS_CONTAINER_LISTENER_H__
#define __HEADLESS_CONTAINER_LISTENER_H__

#include <functional>
#include <memory>
#include <string>
#include <queue>
#include "BinaryData.h"
#include "CoreWallet.h"
#include "EncryptionUtils.h"
#include "ServerConnectionListener.h"
#include "SignerDefs.h"
#include "BSErrorCode.h"
#include "PasswordDialogDataWrapper.h"

#include "headless.pb.h"
#include "bs_signer.pb.h"
#include "Blocksettle_Communication_Internal.pb.h"

namespace spdlog {
   class logger;
}
namespace bs {
   namespace core {
      namespace hd {
         class Leaf;
         class Node;
         class Wallet;
      }
      class WalletsManager;
   }
   class Wallet;
}
class ServerConnection;
class DispatchQueue;

class HeadlessContainerCallbacks
{
public:
   virtual ~HeadlessContainerCallbacks() = default;

   virtual void peerConn(const std::string &) = 0;
   virtual void peerDisconn(const std::string &) = 0;
   virtual void clientDisconn(const std::string &) = 0;

   virtual void decryptWalletRequest(Blocksettle::Communication::signer::PasswordDialogType dialogType
      , const Blocksettle::Communication::Internal::PasswordDialogDataWrapper &dialogData
      , const bs::core::wallet::TXSignRequest & = {}) = 0;

   virtual void txSigned(const BinaryData &) = 0;
   virtual void cancelTxSign(const BinaryData &txId) = 0;
   virtual void updateDialogData(const Blocksettle::Communication::Internal::PasswordDialogDataWrapper &dialogData) = 0;
   virtual void xbtSpent(int64_t, bool) = 0;
   virtual void customDialog(const std::string &, const std::string &) = 0;
   virtual void terminalHandshakeFailed(const std::string &peerAddress) = 0;

   virtual void walletChanged(const std::string &walletId) = 0;

   virtual void ccNamesReceived(bool) = 0;
};

using VoidCb = std::function<void(void)>;
using PasswordReceivedCb = std::function<void(bs::error::ErrorCode result, const SecureBinaryData &password)>;
using PasswordsReceivedCb = std::function<void(const std::unordered_map<std::string, SecureBinaryData> &)>;

struct PasswordRequest
{
   VoidCb passwordRequest;
   PasswordReceivedCb callback;
   Blocksettle::Communication::Internal::PasswordDialogDataWrapper dialogData;
   std::chrono::steady_clock::time_point dialogRequestedTime{std::chrono::steady_clock::now()};

   // dialogs sorted by final time point in ascending order
   // first dialog in vector should be executed firstly
   bool operator < (const PasswordRequest &other) const;
};

class HeadlessContainerListener : public ServerConnectionListener
{
public:
   HeadlessContainerListener(const std::shared_ptr<spdlog::logger> &logger
      , const std::shared_ptr<bs::core::WalletsManager> &
      , const std::shared_ptr<DispatchQueue> &
      , const std::string &walletsPath
      , NetworkType netType
      , const bool &backupEnabled = true);
   ~HeadlessContainerListener() noexcept override;

   void SetLimits(const bs::signer::Limits &limits);

   bool disconnect(const std::string &clientId = {});

   void setCallbacks(HeadlessContainerCallbacks *callbacks);

   void passwordReceived(const std::string &walletId
      , bs::error::ErrorCode result, const SecureBinaryData &password);
   bs::error::ErrorCode activateAutoSign(const std::string &walletId, const SecureBinaryData &password);
   bs::error::ErrorCode deactivateAutoSign(const std::string &walletId = {}, bs::error::ErrorCode reason = bs::error::ErrorCode::NoError);
   //void addPendingAutoSignReq(const std::string &walletId);
   void walletsListUpdated();

   void resetConnection(ServerConnection *connection);

protected:
   bool isAutoSignActive(const std::string &walletId) const;

   void onXbtSpent(const int64_t value, bool autoSign);

   void OnClientConnected(const std::string &clientId) override;
   void OnClientDisconnected(const std::string &clientId) override;
   void OnDataFromClient(const std::string &clientId, const std::string &data) override;
   void OnPeerConnected(const std::string &ip) override;
   void OnPeerDisconnected(const std::string &ip) override;
   void onClientError(const std::string &clientId, ServerConnectionListener::ClientError errorCode, int socket) override;

private:
   void passwordReceived(const std::string &clientId, const std::string &walletId
      , bs::error::ErrorCode result, const SecureBinaryData &password);

   bool sendData(const std::string &data, const std::string &clientId = {});
   bool onRequestPacket(const std::string &clientId, Blocksettle::Communication::headless::RequestPacket packet);

   bool onSignTxRequest(const std::string &clientId, const Blocksettle::Communication::headless::RequestPacket &packet
      , Blocksettle::Communication::headless::RequestType requestType);
   bool onSignMultiTXRequest(const std::string &clientId, const Blocksettle::Communication::headless::RequestPacket &packet);
   bool onSignSettlementPayoutTxRequest(const std::string &clientId
      , const Blocksettle::Communication::headless::RequestPacket &packet);
   bool onSignAuthAddrRevokeRequest(const std::string &clientId, const Blocksettle::Communication::headless::RequestPacket &);
   bool onCreateHDLeaf(const std::string &clientId, Blocksettle::Communication::headless::RequestPacket &packet);
   bool onPromoteHDWallet(const std::string& clientId, Blocksettle::Communication::headless::RequestPacket& packet);
   bool onSetUserId(const std::string &clientId, Blocksettle::Communication::headless::RequestPacket &packet);
   bool onSyncCCNames(Blocksettle::Communication::headless::RequestPacket &packet);
   bool onGetHDWalletInfo(const std::string &clientId, Blocksettle::Communication::headless::RequestPacket &packet);
   bool onCancelSignTx(const std::string &clientId, Blocksettle::Communication::headless::RequestPacket packet);
   bool onUpdateDialogData(const std::string &clientId, Blocksettle::Communication::headless::RequestPacket packet);
   bool onSyncWalletInfo(const std::string &clientId, Blocksettle::Communication::headless::RequestPacket packet);
   bool onSyncHDWallet(const std::string &clientId, Blocksettle::Communication::headless::RequestPacket packet);
   bool onSyncWallet(const std::string &clientId, Blocksettle::Communication::headless::RequestPacket packet);
   bool onSyncComment(const std::string &clientId, Blocksettle::Communication::headless::RequestPacket packet);
   bool onSyncAddresses(const std::string &clientId, Blocksettle::Communication::headless::RequestPacket packet);
   bool onExtAddrChain(const std::string &clientId, Blocksettle::Communication::headless::RequestPacket packet);
   bool onSyncNewAddr(const std::string &clientId, Blocksettle::Communication::headless::RequestPacket packet);
   bool onExecCustomDialog(const std::string &clientId, Blocksettle::Communication::headless::RequestPacket packet);

   bool onCreateSettlWallet(const std::string &clientId, Blocksettle::Communication::headless::RequestPacket packet);
   bool onSetSettlementId(const std::string &clientId, Blocksettle::Communication::headless::RequestPacket packet);
   bool onGetPayinAddr(const std::string &clientId, Blocksettle::Communication::headless::RequestPacket packet);
   bool onSettlGetRootPubkey(const std::string &clientId, Blocksettle::Communication::headless::RequestPacket packet);

   bool AuthResponse(const std::string &clientId, Blocksettle::Communication::headless::RequestPacket packet);
   void SignTXResponse(const std::string &clientId, unsigned int id, Blocksettle::Communication::headless::RequestType reqType
      , bs::error::ErrorCode errorCode, const BinaryData &tx = {});
   void CreateHDLeafResponse(const std::string &clientId, unsigned int id, bs::error::ErrorCode result
      , const std::shared_ptr<bs::core::hd::Leaf>& leaf = nullptr);
   void CreatePromoteHDWalletResponse(const std::string& clientId, unsigned int id, bs::error::ErrorCode result,
                                const std::string& walletId);
   void GetHDWalletInfoResponse(const std::string &clientId, unsigned int id, const std::string &walletId
      , const std::shared_ptr<bs::core::hd::Wallet> &, const std::string &error = {});
   void SyncAddrsResponse(const std::string &clientId, unsigned int id, const std::string &walletId, bs::sync::SyncState);
   void setUserIdResponse(const std::string &clientId, unsigned int id
      , Blocksettle::Communication::headless::AuthWalletResponseType, const std::string &walletId = {});
   void AutoSignActivatedEvent(const std::string &walletId, bool active);

   bool RequestPasswordIfNeeded(const std::string &clientId, const std::string &walletId
      , const bs::core::wallet::TXSignRequest &
      , Blocksettle::Communication::headless::RequestType reqType, const Blocksettle::Communication::Internal::PasswordDialogDataWrapper &dialogData
      , const PasswordReceivedCb &cb);
   bool RequestPasswordsIfNeeded(int reqId, const std::string &clientId
      , const bs::core::wallet::TXMultiSignRequest &, const bs::core::WalletMap &
      , const PasswordsReceivedCb &cb);
   bool RequestPassword(const std::string &rootId, const bs::core::wallet::TXSignRequest &
      , Blocksettle::Communication::headless::RequestType reqType, const Blocksettle::Communication::Internal::PasswordDialogDataWrapper &dialogData
      , const PasswordReceivedCb &cb);
   void RunDeferredPwDialog();

   bool createAuthLeaf(const std::shared_ptr<bs::core::hd::Wallet> &, const BinaryData &salt
      , const SecureBinaryData &password);
   bool createSettlementLeaves(const std::shared_ptr<bs::core::hd::Wallet> &wallet
      , const std::vector<bs::Address> &authAddresses);

   bool CheckSpendLimit(uint64_t value, const std::string &walletId);

private:
   std::shared_ptr<spdlog::logger>     logger_;
   ServerConnection                    *connection_{};
   std::shared_ptr<bs::core::WalletsManager> walletsMgr_;
   std::shared_ptr<DispatchQueue>      queue_;
   const std::string                   walletsPath_;
   const std::string                   backupPath_;
   const NetworkType                   netType_;
   bs::signer::Limits                  limits_;
   std::unordered_set<std::string>     connectedClients_;

   std::unordered_map<std::string, SecureBinaryData>                 passwords_;
   //std::unordered_set<std::string>  autoSignPwdReqs_;

   std::vector<PasswordRequest> deferredPasswordRequests_;
   bool deferredDialogRunning_ = false;

   struct TempPasswords {
      std::unordered_map<std::string, std::unordered_set<std::string>>  rootLeaves;
      std::unordered_set<std::string>  reqWalletIds;
      std::unordered_map<std::string, SecureBinaryData> passwords;
   };
   std::unordered_map<int, TempPasswords> tempPasswords_;
   int reqSeqNo_ = 0;

   const bool backupEnabled_ = true;

   HeadlessContainerCallbacks *callbacks_{};

   std::map<std::pair<std::string, bs::Address>, std::vector<uint32_t>> settlLeafReqs_;
};

#endif // __HEADLESS_CONTAINER_LISTENER_H__
