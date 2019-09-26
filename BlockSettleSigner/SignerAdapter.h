#ifndef SIGNER_ADAPTER_H
#define SIGNER_ADAPTER_H

#include <memory>
#include <QObject>

#include "BSErrorCode.h"
#include "CoreWallet.h"
#include "QmlBridge.h"
#include "QmlFactory.h"
#include "QPasswordData.h"
#include "SignerDefs.h"

#include "bs_signer.pb.h"


namespace bs {
   namespace sync {
      class WalletsManager;
   }
}
namespace spdlog {
   class logger;
}
namespace Blocksettle {
   namespace Communication {
      namespace signer {
         class Settings;
      }
   }
}

class SignAdapterContainer;
class SignerInterfaceListener;

class SignerAdapter : public QObject
{
   Q_OBJECT
   friend class SignerInterfaceListener;

public:
   SignerAdapter(const std::shared_ptr<spdlog::logger> &logger
      , const std::shared_ptr<QmlBridge> &qmlBridge
      , const NetworkType netType, int signerPort, const BinaryData* inSrvIDKey = nullptr);
   ~SignerAdapter() override;

   SignerAdapter(const SignerAdapter&) = delete;
   SignerAdapter& operator = (const SignerAdapter&) = delete;
   SignerAdapter(SignerAdapter&&) = delete;
   SignerAdapter& operator = (SignerAdapter&&) = delete;

   std::shared_ptr<bs::sync::WalletsManager> getWalletsManager();
   void reloadWallets(const QString &, const std::function<void()> &);
   void updateWallet(const std::string &walletId);

   void setLimits(bs::signer::Limits);
   void passwordReceived(const std::string &walletId, bs::error::ErrorCode result, const SecureBinaryData &);

   using ResultCb = std::function<void(bool, const std::string&)>;
   void createWallet(const std::string &name, const std::string &desc, bs::core::wallet::Seed
      , bool primary, const bs::wallet::PasswordData &pwdData
      , const std::function<void(bs::error::ErrorCode)> &cb);

   using CreateWoCb = std::function<void(const bs::sync::WatchingOnlyWallet &)>;
   void importWoWallet(const std::string &filename, const BinaryData &content, const CreateWoCb &cb);

   using ExportWoCb = std::function<void(const BinaryData &content)>;
   void exportWoWallet(const std::string &rootWalletId, const ExportWoCb &cb);

   void deleteWallet(const std::string &rootWalletId, const std::function<void(bool, const std::string&)> &cb);

   void syncSettings(const std::unique_ptr<Blocksettle::Communication::signer::Settings> &);

   void changePassword(const std::string &walletId, const std::vector<bs::wallet::PasswordData> &newPass
      , const bs::wallet::PasswordData &oldPass, bool addNew, bool removeOld
      , const std::function<void(bs::error::ErrorCode errorCode)> &);

   void signOfflineTxRequest(const bs::core::wallet::TXSignRequest &, const SecureBinaryData &password
      , const std::function<void(bs::error::ErrorCode result, const BinaryData &)> &);
   void createWatchingOnlyWallet(const QString &walletId, const SecureBinaryData &password
      , const std::function<void(const SecureBinaryData &privKey, const SecureBinaryData &chainCode)> &);
   void getDecryptedRootNode(const std::string &walletId, const SecureBinaryData &password
      , const std::function<void(const SecureBinaryData &privKey, const SecureBinaryData &chainCode)> &
      , Blocksettle::Communication::signer::PacketType pt = Blocksettle::Communication::signer::GetDecryptedNodeType);
   void getHeadlessPubKey(const std::function<void(const std::string &)> &);

   void activateAutoSign(const std::string &walletId
      , bs::wallet::QPasswordData *passwordData
      , bool activate
      , const std::function<void(bs::error::ErrorCode errorCode)> &cb);

   NetworkType netType() const { return netType_; }

   void setCloseHeadless(bool value) { closeHeadless_ = value; }

   void walletsListUpdated();

   QString headlessPubKey() const;

   void setQmlFactory(const std::shared_ptr<QmlFactory> &qmlFactory);

   std::shared_ptr<QmlBridge> qmlBridge() const;
   std::shared_ptr<QmlFactory> qmlFactory() const;

signals:
   void ready() const;
   void connectionError() const;
   void headlessBindUpdated(bs::signer::BindStatus status) const;
   void peerConnected(const QString &ip);
   void peerDisconnected(const QString &ip);
   void cancelTxSign(const BinaryData &txId);
   void txSigned(const BinaryData &);
   void xbtSpent(const qint64 value, bool autoSign);
   void autoSignActivated(const std::string &walletId);
   void autoSignDeactivated(const std::string &walletId);
   void customDialogRequest(const QString &dialogName, const QVariantMap &data);
   void bindFailed() const;
   void headlessPubKeyChanged(const QString &headlessPubKey) const;
   void terminalHandshakeFailed(const std::string &peerAddress);
   void signerPubKeyUpdated(const BinaryData &pubKey) const;
   void ccInfoReceived(bool) const;

private:
   std::shared_ptr<spdlog::logger>  logger_;
   NetworkType                      netType_;

   std::shared_ptr<SignAdapterContainer>     signContainer_;
   std::shared_ptr<bs::sync::WalletsManager> walletsMgr_;
   std::shared_ptr<QmlFactory>               qmlFactory_;
   std::shared_ptr<SignerInterfaceListener>  listener_;
   std::shared_ptr<QmlBridge>                qmlBridge_;
   bool closeHeadless_{true};

   QString headlessPubKey_;
};


#endif // SIGNER_ADAPTER_H
