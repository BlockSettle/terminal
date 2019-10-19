#ifndef INPROC_SIGNER_H
#define INPROC_SIGNER_H

#include "WalletSignerContainer.h"
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


class InprocSigner : public WalletSignerContainer
{
   Q_OBJECT
public:
   InprocSigner(const std::shared_ptr<bs::core::WalletsManager> &
      , const std::shared_ptr<spdlog::logger> &
      , const std::string &walletsPath, NetworkType);
   InprocSigner(const std::shared_ptr<bs::core::hd::Wallet> &
      , const std::shared_ptr<spdlog::logger> &);
   ~InprocSigner() noexcept override = default;

   bool Start() override;
   bool Stop() override { return true; }
   bool Connect() override { return true; }
   bool Disconnect() override { return true; }
   bool isOffline() const override { return false; }
   bool isWalletOffline(const std::string &) const override { return false; }

   bs::signer::RequestId signTXRequest(const bs::core::wallet::TXSignRequest &
      , TXSignMode mode = TXSignMode::Full, bool keepDuplicatedRecipients = false) override;

   bs::signer::RequestId signSettlementTXRequest(const bs::core::wallet::TXSignRequest &
      , const bs::sync::PasswordDialogData &
      , TXSignMode
      , bool
      , const std::function<void(bs::error::ErrorCode result, const BinaryData &signedTX)> &) override;

   bs::signer::RequestId signSettlementPartialTXRequest(const bs::core::wallet::TXSignRequest &
      , const bs::sync::PasswordDialogData &
      , const std::function<void(bs::error::ErrorCode result, const BinaryData &signedTX)> & ) override { return 0; }

   bs::signer::RequestId signSettlementPayoutTXRequest(const bs::core::wallet::TXSignRequest &
      , const bs::core::wallet::SettlementData &, const bs::sync::PasswordDialogData &
      , const std::function<void(bs::error::ErrorCode, const BinaryData &signedTX)> &) override;

   bs::signer::RequestId signMultiTXRequest(const bs::core::wallet::TXMultiSignRequest &) override;

   bs::signer::RequestId signAuthRevocation(const std::string &walletId, const bs::Address &authAddr
      , const UTXO &, const bs::Address &bsAddr, const SignTxCb &cb = nullptr) override { return 0; }

   bs::signer::RequestId CancelSignTx(const BinaryData &tx) override { return 0; }

   bs::signer::RequestId setUserId(const BinaryData &, const std::string &walletId) override;
   bs::signer::RequestId syncCCNames(const std::vector<std::string> &) override;

   // cb is ignored in inproc signer
   bool createHDLeaf(const std::string &rootWalletId, const bs::hd::Path &path
      , const std::vector<bs::wallet::PasswordData> &pwdData = {}
      , bs::sync::PasswordDialogData dialogData = {}, const CreateHDLeafCb &cb = nullptr) override;
   bool promoteHDWallet(const std::string &, const BinaryData &, bs::sync::PasswordDialogData = {}
      , const PromoteHDWalletCb& = nullptr) override;
   bs::signer::RequestId DeleteHDRoot(const std::string &) override;
   bs::signer::RequestId DeleteHDLeaf(const std::string &) override;
   bs::signer::RequestId GetInfo(const std::string &) override;

   bs::signer::RequestId customDialogRequest(bs::signer::ui::GeneralDialogType, const QVariantMap&) override { return 0; }
   bs::signer::RequestId updateDialogData(const bs::sync::PasswordDialogData &dialogData, uint32_t dialogId = 0) override { return 0; }

   void syncWalletInfo(const std::function<void(std::vector<bs::sync::WalletInfo>)> &) override;
   void syncHDWallet(const std::string &id, const std::function<void(bs::sync::HDWalletData)> &) override;
   void syncWallet(const std::string &id, const std::function<void(bs::sync::WalletData)> &) override;
   void syncAddressComment(const std::string &walletId, const bs::Address &, const std::string &) override;
   void syncTxComment(const std::string &walletId, const BinaryData &, const std::string &) override;
   void extendAddressChain(const std::string &walletId, unsigned count, bool extInt,
      const std::function<void(const std::vector<std::pair<bs::Address, std::string>> &)> &) override;
   void syncAddressBatch(const std::string &walletId, const std::set<BinaryData>& addrSet,
      std::function<void(bs::sync::SyncState)> cb) override;

   void syncNewAddresses(const std::string &walletId, const std::vector<std::string> &
      , const std::function<void(const std::vector<std::pair<bs::Address, std::string>> &)> &
      , bool persistent = true) override;
   void getAddressPreimage(const std::map<std::string, std::vector<bs::Address>> &
      , const std::function<void(const std::map<bs::Address, BinaryData> &)> &) override;

   void createSettlementWallet(const bs::Address &authAddr
      , const std::function<void(const SecureBinaryData &)> &) override;
   void setSettlementID(const std::string &walletId, const SecureBinaryData &id
      , const std::function<void(bool)> &) override;
   void getSettlementPayinAddress(const std::string &walletId
      , const bs::core::wallet::SettlementData &
      , const std::function<void(bool, bs::Address)> &) override;
   void getRootPubkey(const std::string &walletID
      , const std::function<void(bool, const SecureBinaryData &)> &) override;

   bool isReady() const override { return inited_; }

private:
   std::shared_ptr<bs::core::WalletsManager> walletsMgr_;
   const std::string walletsPath_;
   NetworkType       netType_ = NetworkType::Invalid;
   bs::signer::RequestId   seqId_ = 1;
   bool           inited_ = false;
};

#endif // INPROC_SIGNER_H
