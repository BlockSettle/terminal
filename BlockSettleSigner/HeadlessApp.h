#ifndef __HEADLESS_APP_H__
#define __HEADLESS_APP_H__

#include <functional>
#include <memory>
#include <QObject>
#include "CoreWallet.h"
#include "EncryptionUtils.h"
#include "SignContainer.h"
#include "ZMQ_BIP15X_ServerConnection.h"

namespace spdlog {
   class logger;
}
namespace bs {
   namespace core {
      class WalletsManager;
   }
   namespace sync {
      class WalletsManager;
   }
}
class HeadlessContainerListener;
class OfflineProcessor;
class SignerSettings;
class ZmqBIP15XServerConnection;

class HeadlessAppObj : public QObject
{
   Q_OBJECT

public:
   HeadlessAppObj(const std::shared_ptr<spdlog::logger> &
      , const std::shared_ptr<SignerSettings> &);

   void Start();
   void setReadyCallback(const std::function<void(bool)> &cb) { cbReady_ = cb; }
   void setCallbacks(const std::function<void(const std::string &)> &cbPeerConn
      , const std::function<void(const std::string &)> &cbPeerDisconn
      , const std::function<void(const bs::core::wallet::TXSignRequest &, const std::string &)> &cbPwd
      , const std::function<void(const BinaryData &)> &cbTxSigned
      , const std::function<void(const BinaryData &)> &cbCancelTxSign
      , const std::function<void(int64_t, bool)> &cbXbtSpent
      , const std::function<void(const std::string &)> &cbAsAct
      , const std::function<void(const std::string &)> &cbAsDeact);

   std::shared_ptr<bs::sync::WalletsManager> getWalletsManager() const;
   void reloadWallets(const std::string &, const std::function<void()> &);
   void reconnect(const std::string &listenAddr, const std::string &port);
   void setOnline(bool);
   void signTxRequest(const bs::core::wallet::TXSignRequest &, const SecureBinaryData &password
      , const std::function<void(const BinaryData &)> &);
   void createWatchingOnlyWallet(const std::string &walletId, const SecureBinaryData &password
      , std::string path, const std::function<void(bool result)> &);
   void getDecryptedRootNode(const std::string &walletId, const SecureBinaryData &password
      , const std::function<void(const SecureBinaryData &privKey, const SecureBinaryData &chainCode)> &);
   void setLimits(SignContainer::Limits);
   void passwordReceived(const std::string &walletId, const SecureBinaryData &, bool cancelledByUser);

signals:
   void started();
   void finished();

private:
   void OnlineProcessing();
   void OfflineProcessing();

   void setConsoleEcho(bool enable) const;

   std::shared_ptr<spdlog::logger>  logger_;
   const std::shared_ptr<SignerSettings>        settings_;
   std::shared_ptr<bs::core::WalletsManager>    walletsMgr_;
   std::shared_ptr<ZmqBIP15XServerConnection> connection_;
   std::shared_ptr<HeadlessContainerListener>   listener_;
   std::shared_ptr<OfflineProcessor>            offlineProc_;
   SecureBinaryData                             zmqPubKey_;
   SecureBinaryData                             zmqPrvKey_;

   std::function<void(bool)>   cbReady_ = nullptr;
};

#endif // __HEADLESS_APP_H__
