#ifndef __SIGN_CONTAINER_H__
#define __SIGN_CONTAINER_H__

#include <memory>
#include <string>

#include <QObject>
#include <QStringList>
#include <QVariant>

#include "CoreWallet.h"
#include "QWalletInfo.h"

#include "SignerDefs.h"
#include "SignerUiDefs.h"
#include "ZMQ_BIP15X_DataConnection.h"

#include "PasswordDialogData.h"

namespace spdlog {
   class logger;
}
namespace bs {
   namespace sync {
      namespace hd {
         class Leaf;
         class Wallet;
      }
      class SettlementWallet;
      class Wallet;
   }
}
class ApplicationSettings;
class ConnectionManager;

class SignContainer : public QObject
{
   Q_OBJECT
public:
   enum class OpMode {
      Local = 1,
      Remote,
      // RemoteInproc - should be used for testing only, when you need to have signer and listener
      // running in same process and could not use TCP for any reason
      RemoteInproc,
      LocalInproc
   };
   enum class TXSignMode {
      Full,
      Partial
   };
   using PasswordType = SecureBinaryData;

   enum ConnectionError
   {
      NoError,
      UnknownError,
      SocketFailed,
      HostNotFound,
      HandshakeFailed,
      SerializationFailed,
      HeartbeatWaitFailed,
      InvalidProtocol,
      NetworkTypeMismatch,
      ConnectionTimeout,
      SignerGoesOffline,
   };
   Q_ENUM(ConnectionError)

   SignContainer(const std::shared_ptr<spdlog::logger> &, OpMode opMode);
   ~SignContainer() noexcept = default;

   virtual bool Start() { return true; }
   virtual bool Stop() { return true; }
   virtual bool Connect() { return true; }
   virtual bool Disconnect() { return true; }

   using SignTxCb = std::function<void(bs::error::ErrorCode result, const BinaryData &signedTX)>;

   // If wallet is offline serialize request and write to file with path TXSignRequest::offlineFilePath
   virtual bs::signer::RequestId signTXRequest(const bs::core::wallet::TXSignRequest &
      , TXSignMode mode = TXSignMode::Full, bool keepDuplicatedRecipients = false) = 0;

   virtual bs::signer::RequestId signSettlementTXRequest(const bs::core::wallet::TXSignRequest &
      , const bs::sync::PasswordDialogData &dialogData
      , TXSignMode mode = TXSignMode::Full
      , bool keepDuplicatedRecipients = false
      , const std::function<void(bs::error::ErrorCode result, const BinaryData &signedTX)> &cb = nullptr) = 0;

   virtual bs::signer::RequestId signSettlementPartialTXRequest(const bs::core::wallet::TXSignRequest &
      , const bs::sync::PasswordDialogData &dialogData
      , const SignTxCb &cb = nullptr) = 0;

   virtual bs::signer::RequestId signSettlementPayoutTXRequest(const bs::core::wallet::TXSignRequest &
      , const bs::core::wallet::SettlementData &, const bs::sync::PasswordDialogData &dialogData
      , const SignTxCb &cb = nullptr) = 0;

   virtual bs::signer::RequestId signMultiTXRequest(const bs::core::wallet::TXMultiSignRequest &) = 0;

   virtual bs::signer::RequestId signAuthRevocation(const std::string &walletId, const bs::Address &authAddr
      , const UTXO &, const bs::Address &bsAddr, const SignTxCb &cb = nullptr) = 0;

   virtual bs::signer::RequestId updateDialogData(const bs::sync::PasswordDialogData &dialogData, uint32_t dialogId = 0) = 0;

   virtual bs::signer::RequestId CancelSignTx(const BinaryData &txId) = 0;

   virtual bs::signer::RequestId setUserId(const BinaryData &, const std::string &walletId) = 0;
   virtual bs::signer::RequestId syncCCNames(const std::vector<std::string> &) = 0;

   virtual bs::signer::RequestId GetInfo(const std::string &rootWalletId) = 0;

   virtual bs::signer::RequestId customDialogRequest(bs::signer::ui::DialogType signerDialog
      , const QVariantMap &data = QVariantMap()) = 0;

   virtual void syncNewAddress(const std::string &walletId, const std::string &index
      , const std::function<void(const bs::Address &)> &);
   virtual void syncNewAddresses(const std::string &walletId, const std::vector<std::string> &
      , const std::function<void(const std::vector<std::pair<bs::Address, std::string>> &)> &, bool persistent = true) = 0;

   const OpMode &opMode() const { return mode_; }
   virtual bool isReady() const { return true; }
   virtual bool isOffline() const { return true; }
   virtual bool isWalletOffline(const std::string &) const { return true; }

   bool isLocal() const { return mode_ == OpMode::Local || mode_ == OpMode::LocalInproc; }

signals:
   void connected();
   void disconnected();
   void authenticated();
   void connectionError(ConnectionError error, const QString &details);
   void ready();
   void Error(bs::signer::RequestId id, std::string error);
   void TXSigned(bs::signer::RequestId id, BinaryData signedTX, bs::error::ErrorCode result, const std::string &errorReason = {});

   void QWalletInfo(unsigned int id, const bs::hd::WalletInfo &);
   void PasswordChanged(const std::string &walletId, bool success);
   void AutoSignStateChanged(const std::string &walletId, bool active);

protected:
   std::shared_ptr<spdlog::logger> logger_;
   const OpMode mode_;
};


bool SignerConnectionExists(const QString &host, const QString &port);


#endif // __SIGN_CONTAINER_H__
