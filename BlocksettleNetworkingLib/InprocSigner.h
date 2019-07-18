#ifndef INPROC_SIGNER_H
#define INPROC_SIGNER_H

#include "SignContainer.h"
#include <vector>

namespace spdlog {
   class logger;
}
namespace bs {
   namespace core {
      namespace hd {
         class Wallet;
      }
      class SettlementWallet;
      class WalletsManager;
   }
}


class InprocSigner : public SignContainer
{
   Q_OBJECT
public:
   InprocSigner(const std::shared_ptr<bs::core::WalletsManager> &
      , const std::shared_ptr<spdlog::logger> &
      , const std::string &walletsPath, NetworkType);
   InprocSigner(const std::shared_ptr<bs::core::hd::Wallet> &
      , const std::shared_ptr<spdlog::logger> &);
   InprocSigner(const std::shared_ptr<bs::core::SettlementWallet> &
      , const std::shared_ptr<spdlog::logger> &);
   ~InprocSigner() noexcept = default;

   bool Start() override;
   bool Stop() override { return true; }
   bool Connect() override { return true; }
   bool Disconnect() override { return true; }
   bool isOffline() const override { return false; }
   bool isWalletOffline(const std::string &) const override { return false; }

   bs::signer::RequestId signTXRequest(const bs::core::wallet::TXSignRequest &
      , TXSignMode mode = TXSignMode::Full, const PasswordType& password = {}
      , bool keepDuplicatedRecipients = false) override;
   bs::signer::RequestId signPartialTXRequest(const bs::core::wallet::TXSignRequest &
      , const PasswordType& password = {}) override;
   bs::signer::RequestId signPayoutTXRequest(const bs::core::wallet::TXSignRequest &, const bs::Address &authAddr
      , const std::string &settlementId, const PasswordType& password = {}) override;

   bs::signer::RequestId signSettlementTXRequest(const bs::core::wallet::TXSignRequest &
      , const bs::sync::PasswordDialogData &
      , TXSignMode
      , bool
      , const std::function<void(bs::error::ErrorCode result, const BinaryData &signedTX)> &) override { return 0; }

   bs::signer::RequestId signSettlementPartialTXRequest(const bs::core::wallet::TXSignRequest &
      , const bs::sync::PasswordDialogData &
      , const std::function<void(bs::error::ErrorCode result, const BinaryData &signedTX)> & ) override { return 0; }

   bs::signer::RequestId signSettlementPayoutTXRequest(const bs::core::wallet::TXSignRequest &
      , const bs::sync::PasswordDialogData &
      , const bs::Address &, const std::string &
      , const std::function<void(bs::error::ErrorCode , const BinaryData &signedTX)> &)  override { return 0; }

   bs::signer::RequestId signMultiTXRequest(const bs::core::wallet::TXMultiSignRequest &) override;
   bs::signer::RequestId CancelSignTx(const BinaryData &) override { return 0; }

   bs::signer::RequestId setUserId(const BinaryData &, const std::string &walletId) override;
   bs::signer::RequestId syncCCNames(const std::vector<std::string> &) override;

   bs::signer::RequestId createHDLeaf(const std::string &rootWalletId, const bs::hd::Path &
      , const std::vector<bs::wallet::PasswordData> &pwdData = {}
      , const std::function<void(bs::error::ErrorCode result)> &cb = nullptr) override;
   bs::signer::RequestId DeleteHDRoot(const std::string &) override;
   bs::signer::RequestId DeleteHDLeaf(const std::string &) override;
   bs::signer::RequestId GetInfo(const std::string &) override;
   void createSettlementWallet(const std::function<void(const std::shared_ptr<bs::sync::SettlementWallet> &)> &) override;
   bs::signer::RequestId customDialogRequest(bs::signer::ui::DialogType signerDialog, const QVariantMap &data = QVariantMap()) override;

   void syncWalletInfo(const std::function<void(std::vector<bs::sync::WalletInfo>)> &) override;
   void syncHDWallet(const std::string &id, const std::function<void(bs::sync::HDWalletData)> &) override;
   void syncWallet(const std::string &id, const std::function<void(bs::sync::WalletData)> &) override;
   void syncAddressComment(const std::string &walletId, const bs::Address &, const std::string &) override;
   void syncTxComment(const std::string &walletId, const BinaryData &, const std::string &) override;
   void extendAddressChain(const std::string &walletId, unsigned count, bool extInt,
      const std::function<void(const std::vector<std::pair<bs::Address, std::string>> &)> &) override;
   void syncAddressBatch(const std::string &walletId, const std::set<BinaryData>& addrSet,
      std::function<void(bs::sync::SyncState)> cb) override;

   void syncNewAddresses(const std::string &walletId, const std::vector<std::pair<std::string, AddressEntryType>> &
      , const std::function<void(const std::vector<std::pair<bs::Address, std::string>> &)> &
      , bool persistent = true) override;

   bool isReady() const override { return inited_; }

   void setSettlementID(const std::string&, const SecureBinaryData&) override;
   bs::Address getSettlementPayinAddress(const std::string&,
      const SecureBinaryData&, const SecureBinaryData&, bool) const override;
   SecureBinaryData getRootPubkey(const std::string& walletID) const override;

private:
   std::shared_ptr<bs::core::WalletsManager> walletsMgr_;
   const std::string walletsPath_;
   NetworkType       netType_ = NetworkType::Invalid;
   bs::signer::RequestId   seqId_ = 1;
   bool           inited_ = false;
};

#endif // INPROC_SIGNER_H
