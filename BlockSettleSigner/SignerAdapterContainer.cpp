#include "SignerAdapterContainer.h"
#include <spdlog/spdlog.h>
#include <QDataStream>
#include <QFile>
#include <QStandardPaths>
#include "CelerClientConnection.h"
#include "DataConnection.h"
#include "DataConnectionListener.h"
#include "HeadlessApp.h"
#include "Wallets/SyncWalletsManager.h"
#include "ZmqContext.h"
#include "ZMQ_BIP15X_DataConnection.h"
#include "SignerInterfaceListener.h"

#include "bs_signer.pb.h"

using namespace Blocksettle::Communication;

bs::signer::RequestId SignAdapterContainer::signTXRequest(const bs::core::wallet::TXSignRequest &txReq
   , const SecureBinaryData &password)
{
   signer::SignOfflineTxRequest request;
   request.set_password(password.toBinStr());
   auto evt = request.mutable_tx_request();

   evt->set_wallet_id(txReq.walletId);
   for (const auto &input : txReq.inputs) {
      evt->add_inputs(input.serialize().toBinStr());
   }
   for (const auto &recip : txReq.recipients) {
      evt->add_recipients(recip->getSerializedScript().toBinStr());
   }
   evt->set_fee(txReq.fee);
   evt->set_rbf(txReq.RBF);
   if (txReq.change.value) {
      auto change = evt->mutable_change();
      change->set_address(txReq.change.address.display());
      change->set_index(txReq.change.index);
      change->set_value(txReq.change.value);
   }

   return listener_->send(signer::SignOfflineTxRequestType, request.SerializeAsString());
}

bool SignAdapterContainer::createHDLeaf(const std::string& rootWalletId, const bs::hd::Path& path,
                                        const std::vector<bs::wallet::PasswordData>& pwdData, bs::sync::PasswordDialogData dialogData,
                                        const WalletSignerContainer::CreateHDLeafCb& cb)
{
   Q_UNUSED(rootWalletId);
   Q_UNUSED(path);
   Q_UNUSED(pwdData);
   Q_UNUSED(dialogData);
   Q_UNUSED(cb);
   return false;
}

bool SignAdapterContainer::promoteHDWallet(const std::string& rootWalletId, const std::vector<bs::wallet::PasswordData>& pwdData,
                                           bs::sync::PasswordDialogData dialogData, const WalletSignerContainer::PromoteHDWalletCb& cb)
{
   Q_UNUSED(rootWalletId);
   Q_UNUSED(pwdData);
   Q_UNUSED(dialogData);
   Q_UNUSED(cb);
   return false;
}

bs::signer::RequestId SignAdapterContainer::DeleteHDRoot(const std::string &rootWalletId) {
   headless::DeleteHDWalletRequest request;
   request.set_rootwalletid(rootWalletId);
   const auto reqId = listener_->send(signer::DeleteHDWalletType, request.SerializeAsString());
   return reqId;
}

void SignAdapterContainer::syncWalletInfo(const std::function<void(std::vector<bs::sync::WalletInfo>)> &cb)
{
   const auto &cbWrap = [this, cb](std::vector<bs::sync::WalletInfo> wi) {
      woWallets_.clear();
      for (const auto &wallet : wi) {
         if (wallet.watchOnly && (wallet.format == bs::sync::WalletFormat::HD)) {
            woWallets_.insert(wallet.id);
         }
      }
      if (cb) {
         cb(wi);
      }
   };
   const auto reqId = listener_->send(signer::SyncWalletInfoType, "");
   listener_->setWalletInfoCb(reqId, cbWrap);
}

void SignAdapterContainer::syncHDWallet(const std::string &id, const std::function<void(bs::sync::HDWalletData)> &cb)
{
   signer::SyncWalletRequest request;
   request.set_wallet_id(id);
   const auto reqId = listener_->send(signer::SyncHDWalletType, request.SerializeAsString());
   listener_->setHDWalletDataCb(reqId, cb);
}

void SignAdapterContainer::syncWallet(const std::string &id, const std::function<void(bs::sync::WalletData)> &cb)
{
   signer::SyncWalletRequest request;
   request.set_wallet_id(id);
   const auto reqId = listener_->send(signer::SyncWalletType, request.SerializeAsString());
   listener_->setWalletDataCb(reqId, cb);
}
