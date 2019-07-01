#ifndef __HEADLESS_CONTAINER_LISTENER_H__
#define __HEADLESS_CONTAINER_LISTENER_H__

#include <functional>
#include <memory>
#include <string>
#include "BinaryData.h"
#include "CoreWallet.h"
#include "EncryptionUtils.h"
#include "ServerConnectionListener.h"
#include "SignerDefs.h"
#include "BSErrorCode.h"

#include "headless.pb.h"
#include "Blocksettle_Communication_Internal.pb.h"

namespace spdlog {
   class logger;
}
namespace bs {
   namespace core {
      namespace hd {
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
   virtual void requestPasswordForSigningTx(const bs::core::wallet::TXSignRequest &, const std::string &) = 0;
   virtual void requestPasswordForSigningSettlementTx(const bs::core::wallet::TXSignRequest &
      , const Blocksettle::Communication::Internal::SettlementInfo &settlementInfo, const std::string &) = 0;
   virtual void txSigned(const BinaryData &) = 0;
   virtual void cancelTxSign(const BinaryData &) = 0;
   virtual void xbtSpent(int64_t, bool) = 0;
   virtual void customDialog(const std::string &, const std::string &) = 0;
   virtual void terminalHandshakeFailed(const std::string &peerAddress) = 0;
};

class HeadlessContainerListener : public ServerConnectionListener
{
public:
   HeadlessContainerListener(const std::shared_ptr<spdlog::logger> &logger
      , const std::shared_ptr<bs::core::WalletsManager> &
      , const std::shared_ptr<DispatchQueue> &
      , const std::string &walletsPath
      , NetworkType netType
      , bool watchingOnly = false
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
   using PasswordReceivedCb = std::function<void(bs::error::ErrorCode result, const SecureBinaryData &password)>;
   using PasswordsReceivedCb = std::function<void(const std::unordered_map<std::string, SecureBinaryData> &)>;
   void passwordReceived(const std::string &clientId, const std::string &walletId
      , bs::error::ErrorCode result, const SecureBinaryData &password);

   bool sendData(const std::string &data, const std::string &clientId = {});
   bool onRequestPacket(const std::string &clientId, Blocksettle::Communication::headless::RequestPacket packet);

   bool onSignTxRequest(const std::string &clientId, const Blocksettle::Communication::headless::RequestPacket &packet
      , Blocksettle::Communication::headless::RequestType requestType);
   bool onSignPayoutTXRequest(const std::string &clientId, const Blocksettle::Communication::headless::RequestPacket &packet);
   bool onSignMultiTXRequest(const std::string &clientId, const Blocksettle::Communication::headless::RequestPacket &packet);
   bool onPasswordReceived(const std::string &clientId, Blocksettle::Communication::headless::RequestPacket &packet);
   bool onSetUserId(const std::string &clientId, Blocksettle::Communication::headless::RequestPacket &packet);
   bool onCreateHDWallet(const std::string &clientId, Blocksettle::Communication::headless::RequestPacket &packet);
   bool onDeleteHDWallet(Blocksettle::Communication::headless::RequestPacket &packet);
   //bool onSetLimits(const std::string &clientId, Blocksettle::Communication::headless::RequestPacket &packet);
   bool onGetRootKey(const std::string &clientId, Blocksettle::Communication::headless::RequestPacket &packet);
   bool onGetHDWalletInfo(const std::string &clientId, Blocksettle::Communication::headless::RequestPacket &packet);
   bool onCancelSignTx(const std::string &clientId, Blocksettle::Communication::headless::RequestPacket packet);
   bool onSyncWalletInfo(const std::string &clientId, Blocksettle::Communication::headless::RequestPacket packet);
   bool onSyncHDWallet(const std::string &clientId, Blocksettle::Communication::headless::RequestPacket packet);
   bool onSyncWallet(const std::string &clientId, Blocksettle::Communication::headless::RequestPacket packet);
   bool onSyncComment(const std::string &clientId, Blocksettle::Communication::headless::RequestPacket packet);
   bool onSyncAddresses(const std::string &clientId, Blocksettle::Communication::headless::RequestPacket packet);
   bool onExecCustomDialog(const std::string &clientId, Blocksettle::Communication::headless::RequestPacket packet);

   bool AuthResponse(const std::string &clientId, Blocksettle::Communication::headless::RequestPacket packet);
   void SignTXResponse(const std::string &clientId, unsigned int id, Blocksettle::Communication::headless::RequestType reqType
      , bs::error::ErrorCode errorCode, const BinaryData &tx = {});
   void CreateHDWalletResponse(const std::string &clientId, unsigned int id, const std::string &errorOrWalletId
      , const BinaryData &pubKey = {}, const BinaryData &chainCode = {}
      , const std::shared_ptr<bs::core::hd::Wallet> &wallet = nullptr);
   void GetRootKeyResponse(const std::string &clientId, unsigned int id, const std::shared_ptr<bs::core::hd::Node> &
      , const std::string &errorOrId);
   void GetHDWalletInfoResponse(const std::string &clientId, unsigned int id, const std::string &walletId
      , const std::shared_ptr<bs::core::hd::Wallet> &, const std::string &error = {});
   void AutoSignActivatedEvent(const std::string &walletId, bool active);

   bool CreateHDLeaf(const std::string &clientId, unsigned int id, const Blocksettle::Communication::headless::NewHDLeaf &request
      , const std::vector<bs::wallet::PasswordData> &pwdData);
   bool CreateHDWallet(const std::string &clientId, unsigned int id, const Blocksettle::Communication::headless::NewHDWallet &request
      , NetworkType, const std::vector<bs::wallet::PasswordData> &pwdData = {}, bs::wallet::KeyRank keyRank = { 0, 0 });
   bool RequestPasswordIfNeeded(const std::string &clientId, const bs::core::wallet::TXSignRequest &
      , Blocksettle::Communication::headless::RequestType reqType, const Blocksettle::Communication::Internal::SettlementInfo &settlementInfo
      , const std::string &prompt, const PasswordReceivedCb &cb);
   bool RequestPasswordsIfNeeded(int reqId, const std::string &clientId
      , const bs::core::wallet::TXMultiSignRequest &, const bs::core::WalletMap &
      , const std::string &prompt, const PasswordsReceivedCb &cb);
   bool RequestPassword(const std::string &clientId, const bs::core::wallet::TXSignRequest &
      , Blocksettle::Communication::headless::RequestType reqType, const Blocksettle::Communication::Internal::SettlementInfo &settlementInfo
      , const std::string &prompt, const PasswordReceivedCb &cb);

   bool CheckSpendLimit(uint64_t value, const std::string &walletId);

   bool isRequestAllowed(Blocksettle::Communication::headless::RequestType) const;

private:
   std::shared_ptr<spdlog::logger>     logger_;
   ServerConnection                    *connection_{};
   std::shared_ptr<bs::core::WalletsManager> walletsMgr_;
   std::shared_ptr<DispatchQueue>      queue_;
   const std::string                   walletsPath_;
   const std::string                   backupPath_;
   const NetworkType                   netType_;
   bs::signer::Limits                  limits_;
   const bool                          watchingOnly_;
   std::unordered_set<std::string>     connectedClients_;

   std::unordered_map<std::string, std::vector<PasswordReceivedCb>>  passwordCallbacks_; // map<wallet_id, std::vector<PasswordReceivedCb>>
   std::unordered_map<std::string, SecureBinaryData>                 passwords_;
   //std::unordered_set<std::string>  autoSignPwdReqs_;

   struct TempPasswords {
      std::unordered_map<std::string, std::unordered_set<std::string>>  rootLeaves;
      std::unordered_set<std::string>  reqWalletIds;
      std::unordered_map<std::string, SecureBinaryData> passwords;
   };
   std::unordered_map<int, TempPasswords> tempPasswords_;
   int reqSeqNo_ = 0;

   const bool backupEnabled_ = true;

   HeadlessContainerCallbacks *callbacks_{};
};

#endif // __HEADLESS_CONTAINER_LISTENER_H__
