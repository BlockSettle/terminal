#ifndef __OFFLINE_SIGNER_H__
#define __OFFLINE_SIGNER_H__

#include "SignContainer.h"
#include <vector>


namespace spdlog {
   class logger;
}
namespace bs {
   namespace sync {
      namespace hd {
         class Leaf;
         class Wallet;
      }
   }
}


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

   RequestId signTXRequest(const bs::core::wallet::TXSignRequest &, bool autoSign = false
      , TXSignMode mode = TXSignMode::Full, const PasswordType& password = {}
      , bool keepDuplicatedRecipients = false) override;

   RequestId signPartialTXRequest(const bs::core::wallet::TXSignRequest &
      , bool autoSign = false, const PasswordType& password = {}) override
   { return 0; }

   RequestId signPayoutTXRequest(const bs::core::wallet::TXSignRequest &, const bs::Address &authAddr
      , const std::string &settlementId, bool autoSign = false, const PasswordType& password = {}) override
   { return 0; }

   RequestId signMultiTXRequest(const bs::core::wallet::TXMultiSignRequest &) override
   { return 0; }

   void SendPassword(const std::string &walletId, const PasswordType &password, bool) override
   {}

   RequestId CancelSignTx(const BinaryData &txId) override
   { return 0; }

   RequestId SetUserId(const BinaryData &) override { return 0; }
   RequestId createHDLeaf(const std::string &walletId, const bs::hd::Path &, const std::vector<bs::wallet::PasswordData> &pwdData = {}) override { return 0; }
   RequestId createHDWallet(const std::string &name, const std::string &desc
      , bool primary, const bs::core::wallet::Seed &seed, const std::vector<bs::wallet::PasswordData> &pwdData = {}, bs::wallet::KeyRank keyRank = { 0, 0 }) override { return 0; }
   RequestId DeleteHDRoot(const std::string &) override { return 0; }
   RequestId DeleteHDLeaf(const std::string &) override { return 0; }
   RequestId getDecryptedRootKey(const std::string &walletId, const SecureBinaryData &password = {}) override { return 0; }
   RequestId GetInfo(const std::string &) override { return 0; }
   void setLimits(const std::string &walletId, const SecureBinaryData &password, bool autoSign) override {}
   RequestId changePassword(const std::string &walletId, const std::vector<bs::wallet::PasswordData> &newPass
      , bs::wallet::KeyRank, const SecureBinaryData &oldPass
      , bool addNew, bool removeOld, bool dryRun) override { return 0; }

private:
   QString        targetDir_;
   RequestId      seqId_ = 1;
};


std::vector<bs::core::wallet::TXSignRequest> ParseOfflineTXFile(const std::string &data);

#endif // __OFFLINE_SIGNER_H__
