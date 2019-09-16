#include "SignerAdapter.h"
#include <spdlog/spdlog.h>
#include <QDataStream>
#include <QFile>
#include "SignContainer.h"
#include "SystemFileUtils.h"
#include "Wallets/SyncWalletsManager.h"
#include "ZmqContext.h"
#include "ZMQ_BIP15X_DataConnection.h"

#include "SignerAdapterContainer.h"
#include "SignerInterfaceListener.h"

namespace {

   const std::string kLocalAddrV4 = "127.0.0.1";

} // namespace

using namespace Blocksettle::Communication;

SignerAdapter::SignerAdapter(const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<QmlBridge> &qmlBridge
   , const NetworkType netType, int signerPort, const BinaryData* inSrvIDKey)
   : QObject(nullptr)
   ,logger_(logger)
   , netType_(netType)
   , qmlBridge_(qmlBridge)
{
   ZmqBIP15XDataConnectionParams params;
   params.ephemeralPeers = true;
   params.setLocalHeartbeatInterval();

   // When creating the client connection, we need to generate a cookie for the
   // server connection in order to enable verification. We also need to add
   // the key we got on the command line to the list of trusted keys.
   params.cookie = BIP15XCookie::MakeClient;
   params.cookiePath = SystemFilePaths::appDataLocation() + "/" + "adapterClientID";

   auto adapterConn = std::make_shared<ZmqBIP15XDataConnection>(logger, params);
   if (inSrvIDKey) {
      std::string connectAddr = kLocalAddrV4 + ":" + std::to_string(signerPort);
      adapterConn->addAuthPeer(ZmqBIP15XPeer(connectAddr, *inSrvIDKey));

      // Temporary (?) kludge: Sometimes, the key gets checked with "_1" at the
      // end of the checked key name. This should be checked and corrected
      // elsewhere, but for now, add a kludge to keep the code happy.
      connectAddr = kLocalAddrV4 + ":" + std::to_string(signerPort) + "_1";
      adapterConn->addAuthPeer(ZmqBIP15XPeer(connectAddr, *inSrvIDKey));
   }

   listener_ = std::make_shared<SignerInterfaceListener>(logger, qmlBridge_, adapterConn, this);
   if (!adapterConn->openConnection(kLocalAddrV4, std::to_string(signerPort)
      , listener_.get())) {
      throw std::runtime_error("adapter connection failed");
   }

   signContainer_ = std::make_shared<SignAdapterContainer>(logger_, listener_);
}

SignerAdapter::~SignerAdapter()
{
   if (closeHeadless_) {
      listener_->send(signer::RequestCloseType, "");
      listener_->closeConnection();
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

void SignerAdapter::signOfflineTxRequest(const bs::core::wallet::TXSignRequest &txReq
   , const SecureBinaryData &password, const std::function<void(bs::error::ErrorCode result, const BinaryData &)> &cb)
{
   const auto reqId = signContainer_->signTXRequest(txReq, password);
   listener_->setTxSignCb(reqId, cb);
}

void SignerAdapter::createWatchingOnlyWallet(const QString &walletId, const SecureBinaryData &password
   , const std::function<void(const SecureBinaryData &privKey, const SecureBinaryData &chainCode)> &cb)
{
   getDecryptedRootNode(walletId.toStdString(), password, cb, signer::CreateWOType);
}

void SignerAdapter::getDecryptedRootNode(const std::string &walletId, const SecureBinaryData &password
   , const std::function<void(const SecureBinaryData &privKey, const SecureBinaryData &chainCode)> &cb
   , signer::PacketType pt)
{
   signer::DecryptWalletEvent request;
   request.set_wallet_id(walletId);
   request.set_password(password.toBinStr());
   const auto reqId = listener_->send(pt, request.SerializeAsString());
   listener_->setDecryptNodeCb(reqId, cb);
}

void SignerAdapter::reloadWallets(const QString &walletsDir, const std::function<void()> &cb)
{
   signer::ReloadWalletsRequest request;
   request.set_path(walletsDir.toStdString());
   const auto reqId = listener_->send(signer::ReloadWalletsType, request.SerializeAsString());
   listener_->setReloadWalletsCb(reqId, cb);
}

void SignerAdapter::updateWallet(const std::string &walletId)
{
   const auto wallet = walletsMgr_->getWalletById(walletId);
   if (!wallet) {
      logger_->debug("[{}] looks like a new wallet was added - syncing all of them");
      walletsListUpdated();
      return;
   }
   wallet->synchronize([this, walletId] {
      logger_->debug("[SignerAdapter::updateWallet] wallet {} was re-synchronized", walletId);
   });
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

void SignerAdapter::syncSettings(const std::unique_ptr<Blocksettle::Communication::signer::Settings> &settings)
{
   listener_->send(signer::SyncSettingsRequestType, settings->SerializeAsString());
}

void SignerAdapter::passwordReceived(const std::string &walletId
   , bs::error::ErrorCode result, const SecureBinaryData &password)
{
   signer::DecryptWalletEvent request;
   request.set_wallet_id(walletId);
   request.set_password(password.toBinStr());
   request.set_errorcode(static_cast<uint32_t>(result));
   listener_->send(signer::PasswordReceivedType, request.SerializeAsString());
}

void SignerAdapter::createWallet(const std::string &name, const std::string &desc
   , bs::core::wallet::Seed seed, bool primary, const std::vector<bs::wallet::PasswordData> &pwdData
   , bs::wallet::KeyRank keyRank, const std::function<void(bs::error::ErrorCode errorCode)> &cb)
{
   headless::CreateHDWalletRequest request;

   if (!pwdData.empty()) {
      request.set_rankm(keyRank.first);
      request.set_rankn(keyRank.second);
   }
   for (const auto &pwd : pwdData) {
      auto reqPwd = request.add_password();
      reqPwd->set_password(pwd.password.toBinStr());
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
      if (!seed.seed().isNull()) {
         wallet->set_seed(seed.seed().toBinStr());
      }
      else if (seed.hasPrivateKey()) {
         wallet->set_privatekey(seed.toXpriv().toBinStr());
      }
   }
   const auto reqId = listener_->send(signer::CreateHDWalletType, request.SerializeAsString());
   listener_->setCreateHDWalletCb(reqId, cb);
}

void SignerAdapter::importWoWallet(const std::string &filename, const BinaryData &content, const CreateWoCb &cb)
{
   signer::ImportWoWalletRequest request;
   request.set_filename(filename);
   request.set_content(content.toBinStr());
   const auto reqId = listener_->send(signer::ImportWoWalletType, request.SerializeAsString());
   listener_->setWatchOnlyCb(reqId, cb);
}

void SignerAdapter::exportWoWallet(const std::string &rootWalletId, const SignerAdapter::ExportWoCb &cb)
{
   signer::ExportWoWalletRequest request;
   request.set_rootwalletid(rootWalletId);
   const auto reqId = listener_->send(signer::ExportWoWalletType, request.SerializeAsString());
   listener_->setExportWatchOnlyCb(reqId, cb);
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
      request.set_oldpassword(oldPass.toBinStr());
   }
   for (const auto &pwd : newPass) {
      auto reqNewPass = request.add_newpassword();
      reqNewPass->set_password(pwd.password.toBinStr());
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

void SignerAdapter::activateAutoSign(const std::string &walletId
   , bs::wallet::QPasswordData *passwordData
   , bool activate
   , const std::function<void(bs::error::ErrorCode errorCode)> &cb)
{
   signer::AutoSignActRequest request;
   request.set_rootwalletid(walletId);
   if (passwordData) {
      request.set_password(passwordData->binaryPassword().toBinStr());
   }
   request.set_activateautosign(activate);
   const auto reqId = listener_->send(signer::AutoSignActType, request.SerializeAsString());
   listener_->setAutoSignCb(reqId, cb);
}

void SignerAdapter::walletsListUpdated()
{
   logger_->debug("[{}]", __func__);
   getWalletsManager()->reset();
   getWalletsManager()->syncWallets();
}

QString SignerAdapter::headlessPubKey() const
{
   return headlessPubKey_;
}

void SignerAdapter::setQmlFactory(const std::shared_ptr<QmlFactory> &qmlFactory)
{
   qmlFactory_ = qmlFactory;
   listener_->setQmlFactory(qmlFactory);
}

std::shared_ptr<QmlBridge> SignerAdapter::qmlBridge() const
{
   return qmlBridge_;
}

std::shared_ptr<QmlFactory> SignerAdapter::qmlFactory() const
{
   return qmlFactory_;
}
