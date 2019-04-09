#ifndef __OFFLINE_SIGNER_H__
#define __OFFLINE_SIGNER_H__

#include <vector>
#include "HeadlessContainer.h"


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


class OfflineSigner : public LocalSigner
{
   Q_OBJECT
public:
   OfflineSigner(const std::shared_ptr<spdlog::logger> &
      , const QString &homeDir, NetworkType, const QString &port
      , const std::shared_ptr<ConnectionManager> &
      , const std::shared_ptr<ApplicationSettings> &);
   ~OfflineSigner() noexcept = default;

   OfflineSigner(const OfflineSigner&) = delete;
   OfflineSigner& operator = (const OfflineSigner&) = delete;
   OfflineSigner(OfflineSigner&&) = delete;
   OfflineSigner& operator = (OfflineSigner&&) = delete;

   void setTargetDir(const QString& targetDir);
   QString targetDir() const;

   bool isOffline() const override { return true; }
   bool isWalletOffline(const std::string &) const override { return true; }

   RequestId signTXRequest(const bs::core::wallet::TXSignRequest &, bool autoSign = false
      , TXSignMode mode = TXSignMode::Full, const PasswordType& password = {}
      , bool keepDuplicatedRecipients = false) override;

   RequestId signPartialTXRequest(const bs::core::wallet::TXSignRequest &
      , bool  = false, const PasswordType&  = {}) override
   { return 0; }

   RequestId signPayoutTXRequest(const bs::core::wallet::TXSignRequest &, const bs::Address &authAddr
      , const std::string &settlementId, bool = false, const PasswordType& = {}) override
   { return 0; }

   RequestId signMultiTXRequest(const bs::core::wallet::TXMultiSignRequest &) override
   { return 0; }

   void SendPassword(const std::string &, const PasswordType &, bool) override
   {}

   RequestId createHDLeaf(const std::string &, const bs::hd::Path &
      , const std::vector<bs::wallet::PasswordData> & = {}) override { return 0; }
   RequestId createHDWallet(const std::string &, const std::string &
      , bool , const bs::core::wallet::Seed &, const std::vector<bs::wallet::PasswordData> & = {}, bs::wallet::KeyRank  = { 0, 0 }) override { return 0; }
   RequestId getDecryptedRootKey(const std::string &, const SecureBinaryData & = {}) override { return 0; }
   void setLimits(const std::string &, const SecureBinaryData &, bool ) override {}

protected:
   QStringList args() const override;
   QString pidFileName() const override;
};


std::vector<bs::core::wallet::TXSignRequest> ParseOfflineTXFile(const std::string &data);

#endif // __OFFLINE_SIGNER_H__
