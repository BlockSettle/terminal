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
   , bool autoSign, TXSignMode mode, const PasswordType& password, bool keepDuplicatedRecipients)
{
   signer::SignTxRequest request;
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

   return listener_->send(signer::SignTxRequestType, request.SerializeAsString());
}


static void makeCreateHDWalletRequest(const std::string &name, const std::string &desc, bool primary
   , const bs::core::wallet::Seed &seed, const std::vector<bs::wallet::PasswordData> &pwdData, bs::wallet::KeyRank keyRank
   , headless::CreateHDWalletRequest &request)
{
   if (!pwdData.empty()) {
      request.set_rankm(keyRank.first);
      request.set_rankn(keyRank.second);
   }
   for (const auto &pwd : pwdData) {
      auto reqPwd = request.add_password();
      reqPwd->set_password(pwd.password.toHexStr());
      reqPwd->set_enctype(static_cast<uint32_t>(pwd.encType));
      reqPwd->set_enckey(pwd.encKey.toBinStr());
   }
   auto wallet = request.mutable_wallet();
   wallet->set_name(name);
   wallet->set_description(desc);
   wallet->set_nettype((seed.networkType() == NetworkType::TestNet) ? headless::TestNetType : headless::MainNetType);
   if (primary) {
      wallet->set_primary(true);
   }
   if (!seed.empty()) {
      if (seed.hasPrivateKey()) {
         wallet->set_privatekey(seed.privateKey().toBinStr());
         wallet->set_chaincode(seed.chainCode().toBinStr());
      } else if (!seed.seed().isNull()) {
         wallet->set_seed(seed.seed().toBinStr());
      }
   }
}

bs::signer::RequestId SignAdapterContainer::createHDWallet(const std::string &name, const std::string &desc
   , bool primary, const bs::core::wallet::Seed &seed, const std::vector<bs::wallet::PasswordData> &pwdData, bs::wallet::KeyRank keyRank)
{
   headless::CreateHDWalletRequest request;
   makeCreateHDWalletRequest(name, desc, primary, seed, pwdData, keyRank, request);
   const auto reqId = listener_->send(signer::CreateHDWalletType, request.SerializeAsString());
   return reqId;
}

bs::signer::RequestId SignAdapterContainer::DeleteHDRoot(const std::string &rootWalletId) {
   headless::DeleteHDWalletRequest request;
   request.set_rootwalletid(rootWalletId);
   const auto reqId = listener_->send(signer::DeleteHDWalletType, request.SerializeAsString());
   return reqId;
}

void SignAdapterContainer::syncWalletInfo(const std::function<void(std::vector<bs::sync::WalletInfo>)> &cb)
{
   const auto reqId = listener_->send(signer::SyncWalletInfoType, "");
   listener_->setWalleteInfoCb(reqId, cb);
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
