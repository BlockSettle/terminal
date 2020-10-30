/*

***********************************************************************************
* Copyright (C) 2018 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef SIGNER_ADAPTER_H
#define SIGNER_ADAPTER_H


#include "BSErrorCode.h"
#include "CoreWallet.h"
#include "QmlBridge.h"
#include "QmlFactory.h"
#include "QPasswordData.h"
#include "SignerDefs.h"

#include "bs_signer.pb.h"

#include <QObject>

#include <memory>

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
class DataConnection;

class SignerAdapter : public QObject
{
   Q_OBJECT
   friend class SignerInterfaceListener;

public:
   SignerAdapter(const std::shared_ptr<spdlog::logger> &logger
      , const std::shared_ptr<QmlBridge> &qmlBridge
      , const NetworkType netType
      , int signerPort, const BinaryData& inSrvIDKey = {});

   SignerAdapter(const std::shared_ptr<spdlog::logger> &logger
      , const std::shared_ptr<QmlBridge> &qmlBridge
      , const NetworkType netType, int signerPort
      , std::shared_ptr<DataConnection>);

   ~SignerAdapter() override;

   SignerAdapter(const SignerAdapter&) = delete;
   SignerAdapter& operator = (const SignerAdapter&) = delete;
   SignerAdapter(SignerAdapter&&) = delete;
   SignerAdapter& operator = (SignerAdapter&&) = delete;

   static std::shared_ptr<DataConnection> instantiateAdapterConnection(
      const std::shared_ptr<spdlog::logger> &logger
      , int signerPort, const BinaryData& inSrvIDKey);


   std::shared_ptr<bs::sync::WalletsManager> getWalletsManager();
   void updateWallet(const std::string &walletId);

   void setLimits(bs::signer::Limits);
   void passwordReceived(const std::string &walletId, bs::error::ErrorCode result, const SecureBinaryData &);

   using ResultCb = std::function<void(bool, const std::string&)>;
   void createWallet(const std::string &name, const std::string &desc, bs::core::wallet::Seed
      , bool primary, bool createLegacyLeaf, const bs::wallet::PasswordData &pwdData
      , const std::function<void(bs::error::ErrorCode)> &cb);

   using CreateWoCb = std::function<void(const bs::sync::WatchingOnlyWallet &)>;
   void importWoWallet(const std::string &filename, const BinaryData &content, const CreateWoCb &cb);
   void importHwWallet(const bs::core::wallet::HwWalletInfo &walletInfo, const CreateWoCb &cb);

   using ExportWoCb = std::function<void(const BinaryData &content)>;
   void exportWoWallet(const std::string &rootWalletId, const ExportWoCb &cb);

   void deleteWallet(const std::string &rootWalletId, const std::function<void(bool, const std::string&)> &cb);

   void syncSettings(const std::unique_ptr<Blocksettle::Communication::signer::Settings> &);

   void changePassword(const std::string &walletId, const std::vector<bs::wallet::PasswordData> &newPass
      , const bs::wallet::PasswordData &oldPass, bool addNew, bool removeOld
      , const std::function<void(bs::error::ErrorCode errorCode)> &);

   void verifyOfflineTxRequest(const BinaryData &signRequest
      , const std::function<void(bs::error::ErrorCode result)> &);
   void signOfflineTxRequest(const bs::core::wallet::TXSignRequest &, const SecureBinaryData &password
      , const std::function<void(bs::error::ErrorCode result, const BinaryData &)> &);
   void getDecryptedRootNode(const std::string &walletId, const SecureBinaryData &password
      , const std::function<void(const SecureBinaryData &privKey, const SecureBinaryData &chainCode)> &
      , Blocksettle::Communication::signer::PacketType pt = Blocksettle::Communication::signer::GetDecryptedNodeType);

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
   std::shared_ptr<SignAdapterContainer> signContainer() const;

   void sendControlPassword(const bs::wallet::QPasswordData &password);
   void changeControlPassword(const bs::wallet::QPasswordData &oldPassword, const bs::wallet::QPasswordData &newPassword
      , const std::function<void(bs::error::ErrorCode errorCode)> &cb);
   void sendWindowStatus(bool visible);

signals:
   void ready() const;
   void connectionError() const;
   void headlessBindUpdated(bs::signer::BindStatus status) const;
   void peerConnected(const std::string &clientId, const std::string &ip, const std::string &publicKey);
   void peerDisconnected(const std::string &clientId);
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
