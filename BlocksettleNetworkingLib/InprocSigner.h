#ifndef INPROC_SIGNER_H
#define INPROC_SIGNER_H

#include "SignContainer.h"
#include <vector>

namespace spdlog {
   class logger;
}
namespace bs {
   namespace core {
      class WalletsManager;
   }
   namespace hd {
      class Wallet;
   }
}


class InprocSigner : public SignContainer
{
   Q_OBJECT
public:
   InprocSigner(const std::shared_ptr<bs::core::WalletsManager> &
      , const std::shared_ptr<spdlog::logger> &
      , const std::string &walletsPath, NetworkType);
   ~InprocSigner() noexcept = default;

   InprocSigner(const InprocSigner&) = delete;
   InprocSigner& operator = (const InprocSigner&) = delete;
   InprocSigner(InprocSigner&&) = delete;
   InprocSigner& operator = (InprocSigner&&) = delete;

   bool Start() override;
   bool Stop() override { return true; }
   bool Connect() override { return true; }
   bool Disconnect() override { return true; }
   bool isOffline() const override { return false; }
   bool isWalletOffline(const std::string &walletId) const override { return false; }

   RequestId SignTXRequest(const bs::wallet::TXSignRequest &, bool autoSign = false
      , TXSignMode mode = TXSignMode::Full, const PasswordType& password = {}
      , bool keepDuplicatedRecipients = false) override;

   RequestId SignPartialTXRequest(const bs::wallet::TXSignRequest &
      , bool autoSign = false, const PasswordType& password = {}) override {
      return 0;
   }
   RequestId SignPayoutTXRequest(const bs::wallet::TXSignRequest &, const bs::Address &authAddr
      , const std::shared_ptr<bs::core::SettlementAddressEntry> &
      , bool autoSign = false, const PasswordType& password = {}) override {
      return 0;
   }
   RequestId SignMultiTXRequest(const bs::wallet::TXMultiSignRequest &) override { return 0; }
   RequestId CancelSignTx(const BinaryData &txId) override { return 0; }
   void SendPassword(const std::string &walletId, const PasswordType &password, bool) override {}

   RequestId SetUserId(const BinaryData &) override { return 0; }
   RequestId SyncAddresses(const std::vector<std::pair<std::shared_ptr<bs::Wallet>
      , bs::Address>> &) override { return 0; }
   RequestId CreateHDLeaf(const std::shared_ptr<bs::hd::Wallet> &, const bs::hd::Path &
      , const std::vector<bs::wallet::PasswordData> &pwdData = {}) override { return 0; }
   RequestId CreateHDWallet(const std::string &name, const std::string &desc
      , bool primary, const bs::core::wallet::Seed &seed, const std::vector<bs::wallet::PasswordData> &pwdData = {}
      , bs::wallet::KeyRank keyRank = { 0, 0 }) override;
   RequestId DeleteHDRoot(const std::string &) override { return 0; }
   RequestId DeleteHDLeaf(const std::string &) override { return 0; }
   RequestId GetDecryptedRootKey(const std::shared_ptr<bs::hd::Wallet> &,
      const SecureBinaryData &password = {}) override { return 0; }
   RequestId GetInfo(const std::string &) override { return 0; }
   void SetLimits(const std::shared_ptr<bs::hd::Wallet> &, const SecureBinaryData &password, bool autoSign) override {}
   RequestId ChangePassword(const std::shared_ptr<bs::hd::Wallet> &, const std::vector<bs::wallet::PasswordData> &newPass
      , bs::wallet::KeyRank, const SecureBinaryData &oldPass
      , bool addNew, bool removeOld, bool dryRun) override { return 0; }

   void syncWalletInfo(const std::function<void(std::vector<bs::sync::WalletInfo>)> &) override;
   void syncHDWallet(const std::string &id, const std::function<void(bs::sync::HDWalletData)> &) override;
   void syncWallet(const std::string &id, const std::function<void(bs::sync::WalletData)> &) override;

   bool isReady() const override { return true; }

private:
   std::shared_ptr<bs::core::WalletsManager> walletsMgr_;
   const std::string walletsPath_;
   NetworkType       netType_ = NetworkType::Invalid;
   RequestId      seqId_ = 1;
};

#endif // INPROC_SIGNER_H
