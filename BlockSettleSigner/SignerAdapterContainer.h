#ifndef SIGNER_ADAPTER_CONTAINER_H
#define SIGNER_ADAPTER_CONTAINER_H

#include <memory>
#include <QObject>
#include "CoreWallet.h"
#include "SignerAdapter.h"

namespace bs {
   namespace sync {
      class WalletsManager;
   }
}
namespace spdlog {
   class logger;
}

class SignAdapterContainer : public SignContainer
{
public:
   SignAdapterContainer(const std::shared_ptr<spdlog::logger> &logger
      , const std::shared_ptr<SignerInterfaceListener> &lsn)
      : SignContainer(logger, OpMode::LocalInproc), listener_(lsn)
   {}
   ~SignAdapterContainer() noexcept = default;

   RequestId signTXRequest(const bs::core::wallet::TXSignRequest &, bool autoSign = false
      , TXSignMode mode = TXSignMode::Full, const PasswordType& password = {}
   , bool keepDuplicatedRecipients = false) override;
   RequestId signPartialTXRequest(const bs::core::wallet::TXSignRequest &
      , bool autoSign = false, const PasswordType& password = {}) override { return 0; }
   RequestId signPayoutTXRequest(const bs::core::wallet::TXSignRequest &, const bs::Address &authAddr
      , const std::string &settlementId, bool autoSign = false, const PasswordType& password = {}) override {
      return 0;
   }
   RequestId signMultiTXRequest(const bs::core::wallet::TXMultiSignRequest &) override { return 0; }

   void SendPassword(const std::string &walletId, const PasswordType &password,
      bool cancelledByUser) override {}
   RequestId CancelSignTx(const BinaryData &txId) override { return 0; }

   RequestId SetUserId(const BinaryData &) override { return 0; }
   RequestId createHDLeaf(const std::string &rootWalletId, const bs::hd::Path &
      , const std::vector<bs::wallet::PasswordData> &pwdData = {}) override { return 0; }
   RequestId createHDWallet(const std::string &name, const std::string &desc
      , bool primary, const bs::core::wallet::Seed &
      , const std::vector<bs::wallet::PasswordData> &pwdData = {}, bs::wallet::KeyRank keyRank = { 0, 0 }) override {
      return 0;
   }
   RequestId DeleteHDRoot(const std::string &rootWalletId) override { return 0; }
   RequestId DeleteHDLeaf(const std::string &leafWalletId) override { return 0; }
   RequestId getDecryptedRootKey(const std::string &walletId, const SecureBinaryData &password = {}) override { return 0; }
   RequestId GetInfo(const std::string &rootWalletId) override { return 0; }
   void setLimits(const std::string &walletId, const SecureBinaryData &password, bool autoSign) override {}
   RequestId customDialogRequest(bs::signer::ui::DialogType signerDialog, const QVariantMap &data = QVariantMap()) override  { return 0; }

   void syncWalletInfo(const std::function<void(std::vector<bs::sync::WalletInfo>)> &) override;
   void syncHDWallet(const std::string &id, const std::function<void(bs::sync::HDWalletData)> &) override;
   void syncWallet(const std::string &id, const std::function<void(bs::sync::WalletData)> &) override;
   void syncAddressComment(const std::string &walletId, const bs::Address &, const std::string &) override {}
   void syncTxComment(const std::string &walletId, const BinaryData &, const std::string &) override {}
   void syncNewAddress(const std::string &walletId, const std::string &index, AddressEntryType
      , const std::function<void(const bs::Address &)> &) override {}
   void syncNewAddresses(const std::string &walletId, const std::vector<std::pair<std::string, AddressEntryType>> &
      , const std::function<void(const std::vector<std::pair<bs::Address, std::string>> &)> &, bool persistent = true) override {}

private:
   std::shared_ptr<SignerInterfaceListener>  listener_;
};


#endif // SIGNER_ADAPTER_CONTAINER_H
