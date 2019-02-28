#ifndef __HEADLESS_CONTAINER_LISTENER_H__
#define __HEADLESS_CONTAINER_LISTENER_H__

#include <functional>
#include <memory>
#include <string>
#include "BinaryData.h"
#include "CoreWallet.h"
#include "EncryptionUtils.h"
#include "ServerConnectionListener.h"
#include "SignContainer.h"

#include "headless.pb.h"

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


class HeadlessContainerListener : public QObject, public ServerConnectionListener
{
   Q_OBJECT

public:
   HeadlessContainerListener(const std::shared_ptr<ServerConnection> &conn
      , const std::shared_ptr<spdlog::logger> &logger
      , const std::shared_ptr<bs::core::WalletsManager> &
      , const std::string &walletsPath
      , NetworkType netType
      , const bool &hasUI = false
      , const bool &backupEnabled = true);
   ~HeadlessContainerListener() noexcept override;

   void SetLimits(const SignContainer::Limits &limits);

   bool disconnect(const std::string &clientId = {});

signals:
   void passwordRequired(const bs::core::wallet::TXSignRequest &, const QString &prompt);
   void clientAuthenticated(const std::string &clientId, const std::string &clientInfo);
   void clientDisconnected(const std::string &clientId);
   void txSigned();
   void xbtSpent(const qint64 value, bool autoSign);
   void autoSignActivated(const std::string &walletId);
   void autoSignDeactivated(const std::string &walletId);
   void autoSignRequiresPwd(const std::string &walletId);
   void peerConnected(const QString &ip);
   void peerDisconnected(const QString &ip);
   void cancelSignTx(const BinaryData &txId);

public slots:
   void activateAutoSign(const std::string &clientId, const std::string &walletId, const SecureBinaryData &password);
   void deactivateAutoSign(const std::string &clientId = {}, const std::string &walletId = {}, const std::string &reason = {});
   void addPendingAutoSignReq(const std::string &walletId);
   bool isAutoSignActive(const std::string &walletId) const;
   void passwordReceived(const std::string &walletId
      , const SecureBinaryData &password, bool cancelledByUser);

private slots:
   void onXbtSpent(const qint64 value, bool autoSign);

protected:
   void OnClientConnected(const std::string &clientId) override;
   void OnClientDisconnected(const std::string &clientId) override;
   void OnDataFromClient(const std::string &clientId, const std::string &data) override;
   void OnPeerConnected(const std::string &ip) override;
   void OnPeerDisconnected(const std::string &ip) override;

private:
   using PasswordReceivedCb = std::function<void(const SecureBinaryData &password, bool cancelledByUser)>;
   using PasswordsReceivedCb = std::function<void(const std::unordered_map<std::string, SecureBinaryData> &)>;
   void passwordReceived(const std::string &clientId, const std::string &walletId
      , const SecureBinaryData &password, bool cancelledByUser);

   bool sendData(const std::string &data, const std::string &clientId = {});
   bool onRequestPacket(const std::string &clientId, Blocksettle::Communication::headless::RequestPacket packet);
   bool onSignTXRequest(const std::string &clientId, const Blocksettle::Communication::headless::RequestPacket &packet
      , bool partial = false);
   bool onSignPayoutTXRequest(const std::string &clientId, const Blocksettle::Communication::headless::RequestPacket &packet);
   bool onSignMultiTXRequest(const std::string &clientId, const Blocksettle::Communication::headless::RequestPacket &packet);
   bool onPasswordReceived(const std::string &clientId, Blocksettle::Communication::headless::RequestPacket &packet);
   bool onSetUserId(const std::string &clientId, Blocksettle::Communication::headless::RequestPacket &packet);
   bool onCreateHDWallet(const std::string &clientId, Blocksettle::Communication::headless::RequestPacket &packet);
   bool onDeleteHDWallet(Blocksettle::Communication::headless::RequestPacket &packet);
   bool onSetLimits(const std::string &clientId, Blocksettle::Communication::headless::RequestPacket &packet);
   bool onGetRootKey(const std::string &clientId, Blocksettle::Communication::headless::RequestPacket &packet);
   bool onGetHDWalletInfo(const std::string &clientId, Blocksettle::Communication::headless::RequestPacket &packet);
   bool onChangePassword(const std::string &clientId, Blocksettle::Communication::headless::RequestPacket &packet);
   bool onCancelSignTx(const std::string &clientId, Blocksettle::Communication::headless::RequestPacket packet);
   bool onSyncWalletInfo(const std::string &clientId, Blocksettle::Communication::headless::RequestPacket packet);
   bool onSyncHDWallet(const std::string &clientId, Blocksettle::Communication::headless::RequestPacket packet);
   bool onSyncWallet(const std::string &clientId, Blocksettle::Communication::headless::RequestPacket packet);
   bool onSyncComment(const std::string &clientId, Blocksettle::Communication::headless::RequestPacket packet);
   bool onSyncAddresses(const std::string &clientId, Blocksettle::Communication::headless::RequestPacket packet);

   void AuthResponse(const std::string &clientId, Blocksettle::Communication::headless::RequestPacket packet
      , const std::string &errMsg = {});
   void SignTXResponse(const std::string &clientId, unsigned int id, Blocksettle::Communication::headless::RequestType reqType
      , const std::string &error, const BinaryData &tx = {}, bool cancelledByUser = false);
   void CreateHDWalletResponse(const std::string &clientId, unsigned int id, const std::string &errorOrWalletId
      , const BinaryData &pubKey = {}, const BinaryData &chainCode = {}
      , const std::shared_ptr<bs::core::hd::Wallet> &wallet = nullptr);
   void GetRootKeyResponse(const std::string &clientId, unsigned int id, const std::shared_ptr<bs::core::hd::Node> &
      , const std::string &errorOrId);
   void GetHDWalletInfoResponse(const std::string &clientId, unsigned int id, const std::string &walletId
      , const std::shared_ptr<bs::core::hd::Wallet> &, const std::string &error = {});
   void ChangePasswordResponse(const std::string &clientId, unsigned int id, const std::string &walletId, bool ok);
   void AutoSignActiveResponse(const std::string &clientId, const std::string &walletId, bool active
      , const std::string &error = {}, unsigned int id = 0);

   bool CreateHDLeaf(const std::string &clientId, unsigned int id, const Blocksettle::Communication::headless::NewHDLeaf &request
      , const std::vector<bs::wallet::PasswordData> &pwdData);
   bool CreateHDWallet(const std::string &clientId, unsigned int id, const Blocksettle::Communication::headless::NewHDWallet &request
      , NetworkType, const std::vector<bs::wallet::PasswordData> &pwdData = {}, bs::wallet::KeyRank keyRank = { 0, 0 });
   bool RequestPasswordIfNeeded(const std::string &clientId, const bs::core::wallet::TXSignRequest &
      , const QString &prompt, const PasswordReceivedCb &cb, bool autoSign);
   bool RequestPasswordsIfNeeded(int reqId, const std::string &clientId
      , const bs::core::wallet::TXMultiSignRequest &, const bs::core::WalletMap &
      , const QString &prompt, const PasswordsReceivedCb &cb);
   bool RequestPassword(const std::string &clientId, const bs::core::wallet::TXSignRequest &, const QString &prompt
      , const PasswordReceivedCb &cb);

   bool CheckSpendLimit(uint64_t value, bool autoSign, const std::string &walletId);

   SecureBinaryData authTicket(const std::string &clientId) const;

private:
   std::shared_ptr<ServerConnection>   connection_;
   std::shared_ptr<spdlog::logger>     logger_;
   std::shared_ptr<bs::core::WalletsManager> walletsMgr_;
   const std::string                   walletsPath_;
   const std::string                   backupPath_;
   const NetworkType                   netType_;
   SignContainer::Limits               limits_;
   const bool                          hasUI_;
   std::unordered_map<std::string, SecureBinaryData>  authTickets_;
   std::unordered_set<std::string>     connectedClients_;

   std::unordered_map<std::string, std::vector<PasswordReceivedCb>>  passwordCallbacks_;
   std::unordered_map<std::string, SecureBinaryData>                 passwords_;
   std::unordered_set<std::string>  autoSignPwdReqs_;

   struct TempPasswords {
      std::unordered_map<std::string, std::unordered_set<std::string>>  rootLeaves;
      std::unordered_set<std::string>  reqWalletIds;
      std::unordered_map<std::string, SecureBinaryData> passwords;
   };
   std::unordered_map<int, TempPasswords> tempPasswords_;
   int reqSeqNo_ = 0;

   const bool backupEnabled_ = true;
};

#endif // __HEADLESS_CONTAINER_LISTENER_H__
