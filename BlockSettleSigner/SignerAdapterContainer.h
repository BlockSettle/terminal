/*

***********************************************************************************
* Copyright (C) 2016 - , BlockSettle AB
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
#include "WalletSignerContainer.h"

namespace spdlog {
   class logger;
}

class SignAdapterContainer : public WalletSignerContainer
{
public:
   SignAdapterContainer(const std::shared_ptr<spdlog::logger> &logger
      , const std::shared_ptr<SignerInterfaceListener> &lsn)
      : WalletSignerContainer(logger, OpMode::LocalInproc), listener_(lsn)
   {}
   ~SignAdapterContainer() noexcept override = default;

   // Used to sign offline requests from signer
   bs::signer::RequestId signTXRequest(const bs::core::wallet::TXSignRequest &
      , const SecureBinaryData &password);

   bs::signer::RequestId signTXRequest(const bs::core::wallet::TXSignRequest &
      , TXSignMode = TXSignMode::Full, bool = false) override
   { return 0; }

   void createSettlementWallet(const bs::Address &
      , const std::function<void(const SecureBinaryData &)> &) override {}
   void setSettlementID(const std::string &, const SecureBinaryData &
      , const std::function<void(bool)> &) override {}
   void getSettlementPayinAddress(const std::string &,
      const bs::core::wallet::SettlementData &, const std::function<void(bool, bs::Address)> &) override {}
   void getRootPubkey(const std::string&
      , const std::function<void(bool, const SecureBinaryData &)> &) override {}

   bs::signer::RequestId signSettlementTXRequest(const bs::core::wallet::TXSignRequest &
      , const bs::sync::PasswordDialogData &
      , TXSignMode
      , bool
      , const std::function<void(bs::error::ErrorCode result, const BinaryData &signedTX)> &) override {return 0; }

   bs::signer::RequestId signSettlementPartialTXRequest(const bs::core::wallet::TXSignRequest &
      , const bs::sync::PasswordDialogData &
      , const std::function<void(bs::error::ErrorCode result, const BinaryData &signedTX)> & ) override { return 0; }

   bs::signer::RequestId signSettlementPayoutTXRequest(const bs::core::wallet::TXSignRequest &
      , const bs::core::wallet::SettlementData &, const bs::sync::PasswordDialogData &
      , const std::function<void(bs::error::ErrorCode , const BinaryData &signedTX)> &)  override { return 0; }

   bs::signer::RequestId signAuthRevocation(const std::string &walletId, const bs::Address &authAddr
      , const UTXO &, const bs::Address &bsAddr, const SignTxCb &cb = nullptr) override { return 0; }

   bs::signer::RequestId updateDialogData(const bs::sync::PasswordDialogData &, uint32_t = 0) override { return 0; }
   bs::signer::RequestId CancelSignTx(const BinaryData &tx) override { return 0; }

   bs::signer::RequestId setUserId(const BinaryData &, const std::string &) override { return 0; }
   bs::signer::RequestId syncCCNames(const std::vector<std::string> &) override { return 0; }

   bool createHDLeaf(const std::string&, const bs::hd::Path&
      , const std::vector<bs::wallet::PasswordData>& = {}
         , bs::sync::PasswordDialogData = {}, const CreateHDLeafCb & = nullptr) override { return false; }

   bool promoteHDWallet(const std::string &, const BinaryData &
      , bs::sync::PasswordDialogData = {}, const PromoteHDWalletCb& = nullptr) override { return false; }

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
      , const std::function<void(const std::vector<std::pair<bs::Address, std::string>> &)> &, bool = true) override {}
   void getAddressPreimage(const std::map<std::string, std::vector<bs::Address>> &
      , const std::function<void(const std::map<bs::Address, BinaryData> &)> &) override {}
   void getChatNode(const std::string &walletID, const std::function<void(const BIP32_Node &)> &) override {}
   void setSettlAuthAddr(const std::string &walletId, const BinaryData &, const bs::Address &addr) override {}
   void getSettlAuthAddr(const std::string &walletId, const BinaryData &
      , const std::function<void(const bs::Address &)> &) override {}
   void setSettlCP(const std::string &walletId, const BinaryData &payinHash, const BinaryData &settlId
      , const BinaryData &cpPubKey) override {}
   void getSettlCP(const std::string &walletId, const BinaryData &payinHash
      , const std::function<void(const BinaryData &, const BinaryData &)> &) override {}

   bool isWalletOffline(const std::string &id) const override { return (woWallets_.find(id) != woWallets_.end()); }

private:
   std::shared_ptr<SignerInterfaceListener>  listener_;
   std::unordered_set<std::string>           woWallets_;
};


#endif // SIGNER_ADAPTER_CONTAINER_H
