#include "SignerAdapter.h"
#include <spdlog/spdlog.h>
#include <QDataStream>
#include <QFile>
#include "SignContainer.h"
#include "SystemFileUtils.h"
#include "Wallets/SyncWalletsManager.h"
#include "ZmqContext.h"
#include "ZMQ_BIP15X_DataConnection.h"

#include "bs_signer.pb.h"
#include "SignerAdapterContainer.h"
#include "SignerInterfaceListener.h"

using namespace Blocksettle::Communication;

SignerAdapter::SignerAdapter(const std::shared_ptr<spdlog::logger> &logger, NetworkType netType)
   : QObject(nullptr), logger_(logger), netType_(netType)
{
   const auto zmqContext = std::make_shared<ZmqContext>(logger);
   auto adapterConn = std::make_shared<ZmqBIP15XDataConnection>(logger, true, true);
   adapterConn->SetContext(zmqContext);
   {
      const std::string pubKeyFileName = SystemFilePaths::appDataLocation() + "/interface.pub";
      QFile pubKeyFile(QString::fromStdString(pubKeyFileName));
      if (!pubKeyFile.open(QIODevice::WriteOnly)) {
         throw std::runtime_error("failed to create public key file " + pubKeyFileName);
      }
      pubKeyFile.write(QByteArray::fromStdString(adapterConn->getOwnPubKey().toHexStr()));
   }
   listener_ = std::make_shared<SignerInterfaceListener>(logger, adapterConn, this);
   if (!adapterConn->openConnection("127.0.0.1", "23457", listener_.get())) {
      throw std::runtime_error("adapter connection failed");
   }

   signContainer_ = std::make_shared<SignAdapterContainer>(logger_, listener_);
}

SignerAdapter::~SignerAdapter()
{
   if (closeHeadless_) {
      listener_->send(signer::RequestCloseType, "");
   }
}

std::shared_ptr<bs::sync::WalletsManager> SignerAdapter::getWalletsManager()
{
   if (!walletsMgr_) {
      walletsMgr_ = std::make_shared<bs::sync::WalletsManager>(logger_, nullptr, nullptr);
      signContainer_->Start();
      walletsMgr_->setSignContainer(signContainer_);
   }
   return walletsMgr_;
}

void SignerAdapter::signTxRequest(const bs::core::wallet::TXSignRequest &txReq
   , const SecureBinaryData &password, const std::function<void(const BinaryData &)> &cb)
{
   const auto reqId = signContainer_->signTXRequest(txReq, false, SignContainer::TXSignMode::Full, password, true);
   listener_->setTxSignCb(reqId, cb);
}

void SignerAdapter::createWatchingOnlyWallet(const QString &walletId, const SecureBinaryData &password
   , const std::function<void(const bs::sync::WatchingOnlyWallet &)> &cb)
{
   signer::DecryptWalletRequest request;
   request.set_wallet_id(walletId.toStdString());
   request.set_password(password.toBinStr());
   const auto reqId = listener_->send(signer::CreateWOType, request.SerializeAsString());
   listener_->setWatchOnlyCb(reqId, cb);
}

void SignerAdapter::getDecryptedRootNode(const std::string &walletId, const SecureBinaryData &password
   , const std::function<void(const SecureBinaryData &privKey, const SecureBinaryData &chainCode)> &cb)
{
   signer::DecryptWalletRequest request;
   request.set_wallet_id(walletId);
   request.set_password(password.toBinStr());
   const auto reqId = listener_->send(signer::GetDecryptedNodeType, request.SerializeAsString());
   listener_->setDecryptNodeCb(reqId, cb);
}

void SignerAdapter::reloadWallets(const QString &walletsDir, const std::function<void()> &cb)
{
   signer::ReloadWalletsRequest request;
   request.set_path(walletsDir.toStdString());
   const auto reqId = listener_->send(signer::ReloadWalletsType, request.SerializeAsString());
   listener_->setReloadWalletsCb(reqId, cb);
}

void SignerAdapter::setOnline(bool value)
{
   signer::ReconnectRequest request;
   request.set_online(value);
   listener_->send(signer::ReconnectTerminalType, request.SerializeAsString());
}

void SignerAdapter::reconnect(const QString &address, const QString &port)
{
   signer::ReconnectRequest request;
   request.set_online(true);
   request.set_listen_address(address.toStdString());
   request.set_listen_port(port.toStdString());
   listener_->send(signer::ReconnectTerminalType, request.SerializeAsString());
}

void SignerAdapter::setLimits(bs::signer::Limits limits)
{
   signer::SetLimitsRequest request;
   request.set_auto_sign_satoshis(limits.autoSignSpendXBT);
   request.set_manual_satoshis(limits.manualSpendXBT);
   request.set_auto_sign_time(limits.autoSignTimeS);
   request.set_password_keep_in_mem(limits.manualPassKeepInMemS);
   listener_->send(signer::SetLimitsType, request.SerializeAsString());
}

void SignerAdapter::passwordReceived(const std::string &walletId
   , const SecureBinaryData &password, bool cancelledByUser)
{
   signer::DecryptWalletRequest request;
   request.set_wallet_id(walletId);
   request.set_password(password.toBinStr());
   request.set_cancelled_by_user(cancelledByUser);
   listener_->send(signer::PasswordReceivedType, request.SerializeAsString());
}

void SignerAdapter::createWallet(const std::string &name, const std::string &desc
   , bs::core::wallet::Seed seed, bool primary, const std::vector<bs::wallet::PasswordData> &pwdData
   , bs::wallet::KeyRank keyRank, const std::function<void(bool, const std::string&)> &cb)
{
   headless::CreateHDWalletRequest request;

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
   const auto reqId = listener_->send(signer::CreateHDWalletType, request.SerializeAsString());
   listener_->setCreateHDWalletCb(reqId, cb);
}

void SignerAdapter::deleteWallet(const std::string &rootWalletId, const std::function<void (bool, const std::string &)> &cb)
{
   headless::DeleteHDWalletRequest request;
   request.set_rootwalletid(rootWalletId);
   const auto reqId = listener_->send(signer::DeleteHDWalletType, request.SerializeAsString());
   listener_->setDeleteHDWalletCb(reqId, cb);
}

void SignerAdapter::changePassword(const std::string &walletId, const std::vector<bs::wallet::PasswordData> &newPass
     , bs::wallet::KeyRank keyRank, const SecureBinaryData &oldPass
     , bool addNew, bool removeOld, bool dryRun
     , const std::function<void(bool)> &cb)
{
   if (walletId.empty()) {
      logger_->error("[HeadlessContainer] no walletId for ChangePassword");
      return;
   }
   signer::ChangePasswordRequest request;
   request.set_rootwalletid(walletId);
   if (!oldPass.isNull()) {
      request.set_oldpassword(oldPass.toHexStr());
   }
   for (const auto &pwd : newPass) {
      auto reqNewPass = request.add_newpassword();
      reqNewPass->set_password(pwd.password.toHexStr());
      reqNewPass->set_enctype(static_cast<uint32_t>(pwd.encType));
      reqNewPass->set_enckey(pwd.encKey.toBinStr());
   }
   request.set_rankm(keyRank.first);
   request.set_rankn(keyRank.second);
   request.set_addnew(addNew);
   request.set_removeold(removeOld);
   request.set_dryrun(dryRun);

   const auto reqId = listener_->send(signer::ChangePasswordRequestType, request.SerializeAsString());
   listener_->setChangePwCb(reqId, cb);
}

void SignerAdapter::addPendingAutoSignReq(const std::string &walletId)
{
   signer::AutoSignActEvent request;
   request.set_activated(true);
   request.set_wallet_id(walletId);
   listener_->send(signer::AutoSignActType, request.SerializeAsString());
}

void SignerAdapter::deactivateAutoSign()
{
   signer::AutoSignActEvent request;
   request.set_activated(false);
   listener_->send(signer::AutoSignActType, request.SerializeAsString());
}
