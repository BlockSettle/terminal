/*

***********************************************************************************
* Copyright (C) 2019 - 2021, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "SignerAdapter.h"
#include <spdlog/spdlog.h>
#include <QDataStream>
#include <QFile>

#include "Bip15xDataConnection.h"
#include "Wallets/SignContainer.h"
#include "SignerAdapterContainer.h"
#include "SignerInterfaceListener.h"
#include "SystemFileUtils.h"
#include "TransportBIP15x.h"
#include "Wallets/SyncWalletsManager.h"
#include "WsDataConnection.h"

namespace {

   const std::string kLocalAddrV4 = "127.0.0.1";

   const uint32_t kConnectTimeoutSec = 1;

} // namespace

using namespace Blocksettle::Communication;

SignerAdapter::SignerAdapter(const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<QmlBridge> &qmlBridge
   , const NetworkType netType
   , int signerPort, const BinaryData& inSrvIDKey)
   : SignerAdapter(logger, qmlBridge, netType, signerPort, SignerAdapter::instantiateAdapterConnection(logger, signerPort, inSrvIDKey))
{
}

SignerAdapter::SignerAdapter(const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<QmlBridge> &qmlBridge
   , const NetworkType netType, int signerPort
   , std::shared_ptr<DataConnection> adapterConn)
   : QtHCT(nullptr), logger_(logger), netType_(netType)
   , qmlBridge_(qmlBridge)
{
   listener_ = std::make_shared<SignerInterfaceListener>(logger, qmlBridge_, adapterConn, this);
   if (!adapterConn->openConnection(kLocalAddrV4, std::to_string(signerPort)
      , listener_.get())) {
      throw std::runtime_error("adapter connection failed");
   }
   signContainer_ = std::make_shared<SignAdapterContainer>(logger_, listener_);
}

std::shared_ptr<DataConnection> SignerAdapter::instantiateAdapterConnection(
   const std::shared_ptr<spdlog::logger> &logger
   , int signerPort, const BinaryData& inSrvIDKey)
{
   bs::network::BIP15xParams params;
   params.ephemeralPeers = true;

   //connection from GUI to headless signer should be 2-way
   params.authMode = bs::network::BIP15xAuthMode::TwoWay;

   // When creating the client connection, we need to generate a cookie for the
   // server connection in order to enable verification. We also need to add
   // the key we got on the command line to the list of trusted keys.
   params.cookie = bs::network::BIP15xCookie::MakeClient;

   WsDataConnectionParams wsParams;
   wsParams.timeoutSecs = kConnectTimeoutSec;
   auto wsConnection = std::make_unique<WsDataConnection>(logger, wsParams);
   const auto &bip15xTransport = std::make_shared<bs::network::TransportBIP15xClient>(
      logger, params);

   //add server key if provided
   if (!inSrvIDKey.empty()) {
      std::string connectAddr = kLocalAddrV4 + ":" + std::to_string(signerPort);
      bip15xTransport->addAuthPeer(bs::network::BIP15xPeer(connectAddr, inSrvIDKey));

      // Temporary (?) kludge: Sometimes, the key gets checked with "_1" at the
      // end of the checked key name. This should be checked and corrected
      // elsewhere, but for now, add a kludge to keep the code happy.
      connectAddr = kLocalAddrV4 + ":" + std::to_string(signerPort) + "_1";
      bip15xTransport->addAuthPeer(bs::network::BIP15xPeer(connectAddr, inSrvIDKey));
   }

   auto adapterConn = std::make_shared<Bip15xDataConnection>(
      logger, std::move(wsConnection), bip15xTransport);

   return adapterConn;
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

      connect(walletsMgr_.get(), &bs::sync::WalletsManager::walletsSynchronizationStarted
         , listener_.get(), &SignerInterfaceListener::onWalletsSynchronizationStarted);
      connect(walletsMgr_.get(), &bs::sync::WalletsManager::walletsSynchronized
         , listener_.get(), &SignerInterfaceListener::onWalletsSynchronized);
   }
   return walletsMgr_;
}


void SignerAdapter::verifyOfflineTxRequest(const BinaryData &signRequest, const std::function<void (bs::error::ErrorCode)> &cb)
{
   signer::VerifyOfflineTxRequest request;
   request.set_content(signRequest.toBinStr());
   const auto reqId = listener_->send(signer::VerifyOfflineTxRequestType, request.SerializeAsString());
   listener_->setVerifyOfflineTxRequestCb(reqId, [this, cb](bs::error::ErrorCode errorCode) {
      if (errorCode != bs::error::ErrorCode::NoError) {
         cb(errorCode);
         return;
      }
      // Need to sync wallets because they might be extended and include new addresses
      bool result = getWalletsManager()->syncWallets([cb](int count, int total) {
         if (count == total) {
            cb(bs::error::ErrorCode::NoError);
         }
      });
      if (!result) {
         SPDLOG_LOGGER_ERROR(logger_, "wallets sync failed");
         cb(bs::error::ErrorCode::InternalError);
      }
   });
}

void SignerAdapter::signOfflineTxRequest(const bs::core::wallet::TXSignRequest &txReq
   , const SecureBinaryData &password, const std::function<void(bs::error::ErrorCode result, const BinaryData &)> &cb)
{
   const auto reqId = signContainer_->signTXRequest(txReq, password);
   listener_->setTxSignCb(reqId, cb);
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

void SignerAdapter::updateWallet(const std::string &walletId)
{
   if (!walletsMgr_) {
      return;
   }
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
   , bs::core::wallet::Seed seed, bool primary, bool createLegacyLeaf, const bs::wallet::PasswordData &pwdData
   , const std::function<void(bs::error::ErrorCode errorCode)> &cb)
{
   signer::CreateHDWalletRequest request;

   auto reqPwd = request.mutable_password();
   reqPwd->set_password(pwdData.password.toBinStr());
   reqPwd->set_enctype(static_cast<uint32_t>(pwdData.metaData.encType));
   reqPwd->set_enckey(pwdData.metaData.encKey.toBinStr());

   auto wallet = request.mutable_wallet();
   wallet->set_name(name);
   wallet->set_description(desc);
   wallet->set_testnet(seed.networkType() == NetworkType::TestNet);
   if (primary) {
      wallet->set_primary(true);
   }

   wallet->set_create_legacy_leaf(createLegacyLeaf);

   if (!seed.empty()) {
      wallet->set_seed(seed.seed().toBinStr());
   }
   else if (seed.hasPrivateKey()) {
      wallet->set_privatekey(seed.toXpriv().toBinStr());
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

void SignerAdapter::importHwWallet(const bs::core::wallet::HwWalletInfo &walletInfo, const CreateWoCb &cb)
{
   signer::ImportHwWalletRequest request;
   request.set_wallettype(walletInfo.type);
   request.set_label(walletInfo.label);
   request.set_vendor(walletInfo.vendor);
   request.set_deviceid(walletInfo.deviceId);
   request.set_xpubroot(walletInfo.xpubRoot);
   request.set_xpubnestedsegwit(walletInfo.xpubNestedSegwit);
   request.set_xpubnativesegwit(walletInfo.xpubNativeSegwit);
   request.set_xpublegacy(walletInfo.xpubLegacy);

   const auto reqId = listener_->send(signer::ImportHwWalletType, request.SerializeAsString());
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
   , const bs::wallet::PasswordData &oldPass, bool addNew, bool removeOld
   , const std::function<void(bs::error::ErrorCode errorCode)> &cb)
{
   if (walletId.empty()) {
      logger_->error("[HeadlessContainer] no walletId for ChangePassword");
      return;
   }
   signer::ChangePasswordRequest request;
   request.set_root_wallet_id(walletId);

   auto oldPD = request.mutable_old_password();
   oldPD->set_password(oldPass.password.toBinStr());
   oldPD->set_enctype(static_cast<uint32_t>(oldPass.metaData.encType));
   oldPD->set_enckey(oldPass.metaData.encKey.toBinStr());

   for (const auto &pwd : newPass) {
      auto reqNewPass = request.add_new_password();
      reqNewPass->set_password(pwd.password.toBinStr());
      reqNewPass->set_enctype(static_cast<uint32_t>(pwd.metaData.encType));
      reqNewPass->set_enckey(pwd.metaData.encKey.toBinStr());
   }
   request.set_add_new(addNew);
   request.set_remove_old(removeOld);

   const auto reqId = listener_->send(signer::ChangePasswordType, request.SerializeAsString());
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

std::shared_ptr<SignAdapterContainer> SignerAdapter::signContainer() const
{
   return signContainer_;
}

void SignerAdapter::sendControlPassword(const bs::wallet::QPasswordData &password)
{
   signer::EnterControlPasswordRequest decryptEvent;
   decryptEvent.set_controlpassword(password.binaryPassword().toBinStr());
   listener_->send(signer::ControlPasswordReceivedType, decryptEvent.SerializeAsString());
}

void SignerAdapter::changeControlPassword(const bs::wallet::QPasswordData &oldPassword
   , const bs::wallet::QPasswordData &newPassword
   , const std::function<void(bs::error::ErrorCode errorCode)> &cb = nullptr)
{
   signer::ChangeControlPasswordRequest request;
   request.set_controlpasswordold(oldPassword.binaryPassword().toBinStr());
   request.set_controlpasswordnew(newPassword.binaryPassword().toBinStr());
   const auto reqId = listener_->send(signer::ChangeControlPasswordType, request.SerializeAsString());

   listener_->setChangeControlPwCb(reqId, cb);
}

void SignerAdapter::sendWindowStatus(bool visible)
{
   headless::WindowStatus msg;
   msg.set_visible(visible);
   listener_->send(signer::WindowStatusType, msg.SerializeAsString());
}
