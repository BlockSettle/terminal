#ifndef SIGNER_ADAPTER_CONTAINER_H
#define SIGNER_ADAPTER_CONTAINER_H

#include <memory>
#include <QObject>
#include "CoreWallet.h"
#include "SignerAdapter.h"
#include "SignContainer.h"

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

   // Used to sign offline requests from signer
   bs::signer::RequestId signTXRequest(const bs::core::wallet::TXSignRequest &
      , TXSignMode mode = TXSignMode::Full, bool keepDuplicatedRecipients = false) override;
   bs::signer::RequestId signPartialTXRequest(const bs::core::wallet::TXSignRequest &) override { return 0; }
/*   bs::signer::RequestId signPayoutTXRequest(const bs::core::wallet::TXSignRequest &, const bs::Address &authAddr
      , const std::string &settlementId, const PasswordType& password = {}) override {
      return 0;
   }*/

   void createSettlementWallet(const bs::Address &authAddr
      , const std::function<void(const SecureBinaryData &)> &) override {}
   void setSettlementID(const std::string &walletId, const SecureBinaryData &id
      , const std::function<void(bool)> &) override {}
   void getSettlementPayinAddress(const std::string &walletId,
      const SecureBinaryData &settlementId, const SecureBinaryData &cpPubKey
      , const std::function<void(bool, bs::Address)> &, bool myFirst = true) override {}
   void getRootPubkey(const std::string &walletID
      , const std::function<void(bool, const SecureBinaryData &)> &) override {}

   bs::signer::RequestId signSettlementTXRequest(const bs::core::wallet::TXSignRequest &
      , const bs::sync::SettlementInfo &
      , TXSignMode
      , bool
      , const std::function<void(bs::error::ErrorCode result, const BinaryData &signedTX)> &) override {return 0; }

   bs::signer::RequestId signSettlementPartialTXRequest(const bs::core::wallet::TXSignRequest &
      , const bs::sync::SettlementInfo &
      , const std::function<void(bs::error::ErrorCode result, const BinaryData &signedTX)> & ) override { return 0; }

   bs::signer::RequestId signSettlementPayoutTXRequest(const bs::core::wallet::TXSignRequest &
      , const bs::sync::SettlementInfo &
      , const bs::Address &, const std::string &
      , const std::function<void(bs::error::ErrorCode , const BinaryData &signedTX)> &)  override { return 0; }

   bs::signer::RequestId signMultiTXRequest(const bs::core::wallet::TXMultiSignRequest &) override { return 0; }
   bs::signer::RequestId CancelSignTx(const BinaryData &txId) override { return 0; }

   bs::signer::RequestId setUserId(const BinaryData &) override { return 0; }
   bs::signer::RequestId syncCCNames(const std::vector<std::string> &) override { return 0; }

   bs::signer::RequestId createHDLeaf(const std::string &rootWalletId, const bs::hd::Path &
      , const std::vector<bs::wallet::PasswordData> &pwdData = {}) override { return 0; }
   bs::signer::RequestId createHDWallet(const std::string &name, const std::string &desc
      , bool primary, const bs::core::wallet::Seed &
      , const std::vector<bs::wallet::PasswordData> &pwdData = {}, bs::wallet::KeyRank keyRank = { 0, 0 }) override;
   bs::signer::RequestId DeleteHDRoot(const std::string &rootWalletId) override;
   bs::signer::RequestId DeleteHDLeaf(const std::string &leafWalletId) override { return 0; }
   bs::signer::RequestId GetInfo(const std::string &rootWalletId) override { return 0; }
   //void setLimits(const std::string &walletId, const SecureBinaryData &password, bool autoSign) override {}
   bs::signer::RequestId customDialogRequest(bs::signer::ui::DialogType signerDialog, const QVariantMap &data = QVariantMap()) override  { return 0; }

   void syncWalletInfo(const std::function<void(std::vector<bs::sync::WalletInfo>)> &) override;
   void syncHDWallet(const std::string &id, const std::function<void(bs::sync::HDWalletData)> &) override;
   void syncWallet(const std::string &id, const std::function<void(bs::sync::WalletData)> &) override;
   void syncAddressComment(const std::string &walletId, const bs::Address &, const std::string &) override {}
   void syncTxComment(const std::string &walletId, const BinaryData &, const std::string &) override {}
   void syncAddressBatch(const std::string &walletId,
      const std::set<BinaryData>& addrSet, std::function<void(bs::sync::SyncState)>) override {}
   void extendAddressChain(const std::string &walletId, unsigned count, bool extInt,
      const std::function<void(const std::vector<std::pair<bs::Address, std::string>> &)> &) override {}
   void syncNewAddresses(const std::string &walletId, const std::vector<std::pair<std::string, AddressEntryType>> &
      , const std::function<void(const std::vector<std::pair<bs::Address, std::string>> &)> &, bool persistent = true) override {}

   bool isWalletOffline(const std::string &id) const override { return (woWallets_.find(id) != woWallets_.end()); }

private:
   std::shared_ptr<SignerInterfaceListener>  listener_;
   std::unordered_set<std::string>           woWallets_;
};


#endif // SIGNER_ADAPTER_CONTAINER_H
