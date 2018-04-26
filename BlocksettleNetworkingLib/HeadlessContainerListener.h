#ifndef __HEADLESS_CONTAINER_LISTENER_H__
#define __HEADLESS_CONTAINER_LISTENER_H__

#include <functional>
#include <memory>
#include <string>
#include "BinaryData.h"
#include "EncryptionUtils.h"
#include "MetaData.h"
#include "ServerConnectionListener.h"
#include "SignContainer.h"

#include "headless.pb.h"

namespace spdlog {
   class logger;
}
namespace bs {
   namespace hd {
      class Wallet;
   }
   class Wallet;
}
class ServerConnection;
class WalletsManager;


class HeadlessContainerListener : public QObject, public ServerConnectionListener
{
   Q_OBJECT

public:
   HeadlessContainerListener(const std::shared_ptr<ServerConnection> &conn
      , const std::shared_ptr<spdlog::logger> &logger
      , const std::shared_ptr<WalletsManager> &walletsMgr
      , const std::string &pwHash = {}
      , bool hasUI = false);
   ~HeadlessContainerListener() noexcept override;

   void SetLimits(const SignContainer::Limits &limits);

signals:
   void passwordRequired(const bs::wallet::TXSignRequest &, const QString &prompt);
   void clientAuthenticated(const std::string &clientId, const std::string &clientInfo);
   void clientDisconnected(const std::string &clientId);
   void txSigned();
   void xbtSpent(const qint64 value, bool autoSign);
   void autoSignActivated(const std::string &walletId);
   void autoSignDeactivated(const std::string &walletId);
   void autoSignRequiresPwd(const std::string &walletId);

public slots:
   void passwordReceived(const std::string &walletId, const std::string &password);
   void activateAutoSign(const std::string &walletId, const std::string &password);
   void deactivateAutoSign(const std::string &walleteId, const std::string &reason = {});
   void addPendingAutoSignReq(const std::string &walletId);
   bool isAutoSignActive(const std::string &walleteId) const;

private slots:
   void onXbtSpent(const qint64 value, bool autoSign);

protected:
   void OnClientConnected(const std::string &clientId) override;
   void OnClientDisconnected(const std::string &clientId) override;
   void OnDataFromClient(const std::string &clientId, const std::string &data) override;

private:
   using PasswordReceivedCb = std::function<void(const std::string &password)>;
   using PasswordsReceivedCb = std::function<void(const std::unordered_map<std::string, std::string> &)>;

   bool disconnect(const std::string &clientId = {});
   bool sendData(const std::string &data, const std::string &clientId = {});
   bool onRequestPacket(const std::string &clientId, Blocksettle::Communication::headless::RequestPacket packet);
   bool onSignTXRequest(const std::string &clientId, const Blocksettle::Communication::headless::RequestPacket &packet
      , bool partial = false);
   bool onSignPayoutTXRequest(const std::string &clientId, const Blocksettle::Communication::headless::RequestPacket &packet);
   bool onSignMultiTXRequest(const std::string &clientId, const Blocksettle::Communication::headless::RequestPacket &packet);
   bool onPasswordReceived(Blocksettle::Communication::headless::RequestPacket &packet);
   bool onSetUserId(const std::string &clientId, Blocksettle::Communication::headless::RequestPacket &packet);
   bool onSyncAddress(const std::string &clientId, Blocksettle::Communication::headless::RequestPacket &packet);
   bool onCreateHDWallet(const std::string &clientId, Blocksettle::Communication::headless::RequestPacket &packet);
   bool onDeleteHDWallet(Blocksettle::Communication::headless::RequestPacket &packet);
   bool onSetLimits(const std::string &clientId, Blocksettle::Communication::headless::RequestPacket &packet);
   bool onGetRootKey(const std::string &clientId, Blocksettle::Communication::headless::RequestPacket &packet);
   bool onGetHDWalletInfo(const std::string &clientId, Blocksettle::Communication::headless::RequestPacket &packet);
   bool onChangePassword(const std::string &clientId, Blocksettle::Communication::headless::RequestPacket &packet);

   void AuthResponse(const std::string &clientId, Blocksettle::Communication::headless::RequestPacket packet
      , const std::string &errMsg = {});
   void SignTXResponse(const std::string &clientId, unsigned int id, Blocksettle::Communication::headless::RequestType reqType
      , const std::string &error, const BinaryData &tx = {});
   void CreateHDWalletResponse(const std::string &clientId, unsigned int id, const std::string &errorOrWalletId
      , const BinaryData &pubKey = {}, const BinaryData &chainCode = {}, const std::shared_ptr<bs::hd::Wallet> &wallet = nullptr);
   void GetRootKeyResponse(const std::string &clientId, unsigned int id, const std::shared_ptr<bs::hd::Node> &
      , const std::string &errorOrId);
   void GetHDWalletInfoResponse(const std::string &clientId, unsigned int id, bool encrypted = false
      , const std::string &error = {});
   void SyncAddrResponse(const std::string &clientId, unsigned int id, const std::set<std::string> &failedWallets
      , const std::vector<std::pair<std::string, std::string>> &failedAddresses);
   void ChangePasswordResponse(const std::string &clientId, unsigned int id, const std::string &walletId, bool ok);
   void AutoSignActiveResponse(const std::string &walletId, bool active, const std::string &error = {}
      , const std::string &clientId = {}, unsigned int id = 0);

   bool CreateHDLeaf(const std::string &clientId, unsigned int id, const Blocksettle::Communication::headless::NewHDLeaf &request
      , const std::string &password);
   bool CreateHDWallet(const std::string &clientId, unsigned int id, const Blocksettle::Communication::headless::NewHDWallet &request
      , const std::string &password);
   bool RequestPasswordIfNeeded(const std::string &clientId, const bs::wallet::TXSignRequest &
      , const QString &prompt, const PasswordReceivedCb &cb, bool autoSign);
   bool RequestPasswordsIfNeeded(int reqId, const std::string &clientId
      , const bs::wallet::TXMultiSignRequest &, const QString &prompt, const PasswordsReceivedCb &cb);
   bool RequestPassword(const std::string &clientId, const bs::wallet::TXSignRequest &, const QString &prompt
      , const PasswordReceivedCb &cb);

   bool CheckSpendLimit(uint64_t value, bool autoSign, const std::string &walletId);

private:
   std::shared_ptr<ServerConnection>   connection_;
   std::shared_ptr<spdlog::logger>     logger_;
   std::shared_ptr<WalletsManager>     walletsMgr_;
   SignContainer::Limits               limits_;
   const std::string                   pwHash_;
   const bool                          hasUI_;
   SecureBinaryData                    authTicket_;
   std::unordered_set<std::string>     connectedClients_;

   std::unordered_map<std::string, std::vector<PasswordReceivedCb>>  passwordCallbacks_;
   std::unordered_map<std::string, std::string> passwords_;
   std::unordered_set<std::string>  autoSignPwdReqs_;

   struct TempPasswords {
      std::unordered_map<std::string, std::unordered_set<std::string>>  rootLeaves;
      std::unordered_set<std::string>  reqWalletIds;
      std::unordered_map<std::string, std::string> passwords;
   };
   std::unordered_map<int, TempPasswords> tempPasswords_;
   int reqSeqNo_ = 0;
};

#endif // __HEADLESS_CONTAINER_LISTENER_H__
