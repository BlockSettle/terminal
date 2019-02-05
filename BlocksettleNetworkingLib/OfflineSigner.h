#ifndef __OFFLINE_SIGNER_H__
#define __OFFLINE_SIGNER_H__

#include "SignContainer.h"
#include <vector>


namespace spdlog {
   class logger;
}
namespace bs {
   class SettlementAddressEntry;
   namespace hd {
      class Wallet;
   }
}
class ApplicationSettings;
class WalletsManager;


class OfflineSigner : public SignContainer
{
   Q_OBJECT
public:
   OfflineSigner(const std::shared_ptr<spdlog::logger> &, const QString &dir);
   ~OfflineSigner() noexcept = default;

   OfflineSigner(const OfflineSigner&) = delete;
   OfflineSigner& operator = (const OfflineSigner&) = delete;
   OfflineSigner(OfflineSigner&&) = delete;
   OfflineSigner& operator = (OfflineSigner&&) = delete;

   void SetTargetDir(const QString& targetDir);

   bool Start() override;
   bool Stop() override { return true; }
   bool Connect() override { return true; }
   bool Disconnect() override { return true; }
   bool isOffline() const override { return true; }
   bool isWalletOffline(const std::string &walletId) const override { return true; }

   RequestId SignTXRequest(const bs::wallet::TXSignRequest &, bool autoSign = false
      , TXSignMode mode = TXSignMode::Full, const PasswordType& password = {}
      , bool keepDuplicatedRecipients = false) override;

   RequestId SignPartialTXRequest(const bs::wallet::TXSignRequest &
      , bool autoSign = false, const PasswordType& password = {}) override
   { return 0; }

   RequestId SignPayoutTXRequest(const bs::wallet::TXSignRequest &, const bs::Address &authAddr
      , const std::shared_ptr<bs::SettlementAddressEntry> &
      , bool autoSign = false, const PasswordType& password = {}) override
   { return 0; }

   RequestId SignMultiTXRequest(const bs::wallet::TXMultiSignRequest &) override
   { return 0; }

   void SendPassword(const std::string &walletId, const PasswordType &password, bool) override
   {}

   RequestId CancelSignTx(const BinaryData &txId) override
   { return 0; }

   RequestId SetUserId(const BinaryData &) override { return 0; }
   RequestId SyncAddresses(const std::vector<std::pair<std::shared_ptr<bs::Wallet>, bs::Address>> &) override { return 0; }
   RequestId CreateHDLeaf(const std::shared_ptr<bs::hd::Wallet> &, const bs::hd::Path &, const std::vector<bs::wallet::PasswordData> &pwdData = {}) override { return 0; }
   RequestId CreateHDWallet(const std::string &name, const std::string &desc
      , bool primary, const bs::wallet::Seed &seed, const std::vector<bs::wallet::PasswordData> &pwdData = {}, bs::wallet::KeyRank keyRank = { 0, 0 }) override { return 0; }
   RequestId DeleteHDRoot(const std::string &) override { return 0; }
   RequestId DeleteHDLeaf(const std::string &) override { return 0; }
   RequestId GetDecryptedRootKey(const std::shared_ptr<bs::hd::Wallet> &, const SecureBinaryData &password = {}) override { return 0; }
   RequestId GetInfo(const std::string &) override { return 0; }
   void SetLimits(const std::shared_ptr<bs::hd::Wallet> &, const SecureBinaryData &password, bool autoSign) override {}
   RequestId ChangePassword(const std::shared_ptr<bs::hd::Wallet> &, const std::vector<bs::wallet::PasswordData> &newPass
      , bs::wallet::KeyRank, const SecureBinaryData &oldPass
      , bool addNew, bool removeOld, bool dryRun) override { return 0; }

   bool isReady() const override { return true; }

private:
   QString        targetDir_;
   RequestId      seqId_ = 1;
};


std::vector<bs::wallet::TXSignRequest> ParseOfflineTXFile(const std::string &data);

#endif // __OFFLINE_SIGNER_H__
