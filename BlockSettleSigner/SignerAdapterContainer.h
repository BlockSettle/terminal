/*

***********************************************************************************
* Copyright (C) 2019 - 2021, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef SIGNER_ADAPTER_CONTAINER_H
#define SIGNER_ADAPTER_CONTAINER_H

#include <memory>
#include <QObject>
#include "CoreWallet.h"
#include "SignerAdapter.h"
#include "Wallets/WalletSignerContainer.h"

namespace spdlog {
   class logger;
}

class SignAdapterContainer : public WalletSignerContainer, public SignerCallbackTarget
{
public:
   SignAdapterContainer(const std::shared_ptr<spdlog::logger> &logger
      , const std::shared_ptr<SignerInterfaceListener> &lsn)
      : WalletSignerContainer(logger, this, OpMode::LocalInproc), listener_(lsn)
   {}
   ~SignAdapterContainer() noexcept override = default;

   // Used to sign offline requests from signer
   bs::signer::RequestId signTXRequest(const bs::core::wallet::TXSignRequest &
      , const SecureBinaryData &password);

   void signTXRequest(const bs::core::wallet::TXSignRequest&
      , const std::function<void(const BinaryData &signedTX, bs::error::ErrorCode
         , const std::string& errorReason)>&
      , TXSignMode mode = TXSignMode::Full, bool keepDuplicatedRecipients = false) override {}

   void getRootPubkey(const std::string&
      , const std::function<void(bool, const SecureBinaryData &)> &) override {}

   bs::signer::RequestId resolvePublicSpenders(const bs::core::wallet::TXSignRequest &
      , const SignerStateCb &) override { return 0; }

   bs::signer::RequestId updateDialogData(const bs::sync::PasswordDialogData &, uint32_t = 0) override { return 0; }
   bs::signer::RequestId CancelSignTx(const BinaryData &tx) override { return 0; }

   bool createHDLeaf(const std::string&, const bs::hd::Path&
      , const std::vector<bs::wallet::PasswordData>& = {}
         , bs::sync::PasswordDialogData = {}, const CreateHDLeafCb & = nullptr) override { return false; }

   bs::signer::RequestId DeleteHDRoot(const std::string &rootWalletId) override;
   bs::signer::RequestId DeleteHDLeaf(const std::string &) override { return 0; }
   bs::signer::RequestId GetInfo(const std::string &) override { return 0; }
   //void setLimits(const std::string &walletId, const SecureBinaryData &password, bool autoSign) override {}
   bs::signer::RequestId customDialogRequest(bs::signer::ui::GeneralDialogType, const QVariantMap& = QVariantMap()) override  { return 0; }

   void syncWalletInfo(const std::function<void(std::vector<bs::sync::WalletInfo>)> &) override;
   void syncHDWallet(const std::string &id, const std::function<void(bs::sync::HDWalletData)> &) override;
   void syncWallet(const std::string &id, const std::function<void(bs::sync::WalletData)> &) override;
   void syncAddressComment(const std::string &, const bs::Address&, const std::string&) override {}
   void syncTxComment(const std::string&, const BinaryData&, const std::string&) override {}
   void syncAddressBatch(const std::string&,
      const std::set<BinaryData>&, std::function<void(bs::sync::SyncState)>) override {}
   void extendAddressChain(const std::string&, unsigned, bool,
      const std::function<void(const std::vector<std::pair<bs::Address, std::string>> &)> &) override {}
   void syncNewAddresses(const std::string &, const std::vector<std::string> &
      , const std::function<void(const std::vector<std::pair<bs::Address, std::string>> &)> &) override {}

   bool isWalletOffline(const std::string &id) const override { return (woWallets_.find(id) != woWallets_.end()); }

   void Start(void) {}
   void Connect(void) {}

private:
   std::shared_ptr<SignerInterfaceListener>  listener_;
   std::unordered_set<std::string>           woWallets_;
};


#endif // SIGNER_ADAPTER_CONTAINER_H
