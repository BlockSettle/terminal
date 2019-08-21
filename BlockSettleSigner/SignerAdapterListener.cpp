#include "SignerAdapterListener.h"
#include <spdlog/spdlog.h>
#include "CoreHDWallet.h"
#include "CoreWalletsManager.h"
#include "DispatchQueue.h"
#include "HeadlessApp.h"
#include "HeadlessSettings.h"
#include "HeadlessContainerListener.h"
#include "HeadlessSettings.h"
#include "ServerConnection.h"
#include "SystemFileUtils.h"
#include "BSErrorCode.h"

using namespace Blocksettle::Communication;

class HeadlessContainerCallbacksImpl : public HeadlessContainerCallbacks
{
public:
   HeadlessContainerCallbacksImpl(SignerAdapterListener *owner)
      : owner_(owner)
   {}

   void peerConn(const std::string &ip) override
   {
      signer::PeerEvent evt;
      evt.set_ip_address(ip);
      owner_->sendData(signer::PeerConnectedType, evt.SerializeAsString());
   }

   void peerDisconn(const std::string &ip) override
   {
      signer::PeerEvent evt;
      evt.set_ip_address(ip);
      owner_->sendData(signer::PeerDisconnectedType, evt.SerializeAsString());
   }

   void clientDisconn(const std::string &) override
   {
      if (owner_->settings_->runMode() == bs::signer::RunMode::litegui) {
         owner_->logger_->info("Quit because terminal disconnected unexpectedly and litegui used");
         owner_->queue_->quit();
      }
   }

   void ccNamesReceived(bool result) override
   {
      signer::TerminalEvent evt;
      evt.set_cc_info_received(result);
      bool rc = owner_->sendData(signer::TerminalEventType, evt.SerializeAsString());
   }

   signer::SignTxRequest createSignTxRequest(const bs::core::wallet::TXSignRequest &txReq, const std::string &prompt)
   {
      signer::SignTxRequest request;
      request.set_wallet_id(txReq.walletId);
      request.set_prompt(prompt);

      for (const auto &input : txReq.inputs) {
         request.add_inputs(input.serialize().toBinStr());
      }
      for (const auto &recip : txReq.recipients) {
         request.add_recipients(recip->getSerializedScript().toBinStr());
      }
      request.set_fee(txReq.fee);
      request.set_rbf(txReq.RBF);
      if (txReq.change.value) {
         auto change = request.mutable_change();
         change->set_address(txReq.change.address.display());
         change->set_index(txReq.change.index);
         change->set_value(txReq.change.value);
      }

      return request;
   }

   void txSigned(const BinaryData &tx) override
   {
      signer::SignTxEvent evt;
      evt.set_signedtx(tx.toBinStr());
      owner_->sendData(signer::TxSignedType, evt.SerializeAsString());
   }

   void cancelTxSign(const BinaryData &txHash, const std::string &settlId) override
   {
      headless::CancelSignTx evt;
      if (txHash.isNull()) {
         evt.set_settlement_id(settlId);
      }
      else {
         evt.set_tx_hash(txHash.toBinStr());
      }
      owner_->sendData(signer::CancelTxSignType, evt.SerializeAsString());
   }

   void updateDialogData(const Blocksettle::Communication::Internal::PasswordDialogDataWrapper &dialogData) override
   {
      headless::UpdateDialogDataRequest request;
      *request.mutable_passworddialogdata() = dialogData;
      owner_->sendData(signer::UpdateDialogDataType, request.SerializeAsString());
   }

   void xbtSpent(int64_t value, bool autoSign) override
   {
      signer::XbtSpentEvent evt;
      evt.set_value(value);
      evt.set_auto_sign(autoSign);
      owner_->sendData(signer::XbtSpentType, evt.SerializeAsString());
   }

   void customDialog(const std::string &dialogName, const std::string &data) override
   {
      signer::CustomDialogRequest evt;
      evt.set_dialogname(dialogName);
      evt.set_variantdata(data);
      owner_->sendData(signer::ExecCustomDialogRequestType, evt.SerializeAsString());
   }

   void terminalHandshakeFailed(const std::string &peerAddress) override
   {
      signer::TerminalEvent evt;
      evt.set_peer_address(peerAddress);
      evt.set_handshake_ok(false);
      owner_->sendData(signer::TerminalEventType, evt.SerializeAsString());
   }

   void decryptWalletRequest(Blocksettle::Communication::signer::PasswordDialogType dialogType
      , const Blocksettle::Communication::Internal::PasswordDialogDataWrapper &dialogData
      , const bs::core::wallet::TXSignRequest &txReq = {}) override
   {
      signer::DecryptWalletRequest request;
      request.set_dialogtype(dialogType);
      *(request.mutable_signtxrequest()) = createSignTxRequest(txReq, {});
      *(request.mutable_passworddialogdata()) = dialogData;

      owner_->sendData(signer::DecryptWalletRequestType, request.SerializeAsString());
   }

   SignerAdapterListener *owner_{};
};

static std::string toHex(const std::string &binData)
{
   return BinaryData(binData).toHexStr();
}

SignerAdapterListener::SignerAdapterListener(HeadlessAppObj *app
   , ZmqBIP15XServerConnection *connection
   , const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<bs::core::WalletsManager> &walletsMgr
   , const std::shared_ptr<DispatchQueue> &queue
   , const std::shared_ptr<HeadlessSettings> &settings)
   : ServerConnectionListener(), app_(app)
   , connection_(connection)
   , logger_(logger)
   , walletsMgr_(walletsMgr)
   , queue_(queue)
   , settings_(settings)
   , callbacks_(new HeadlessContainerCallbacksImpl(this))
{
}

SignerAdapterListener::~SignerAdapterListener() noexcept = default;

void SignerAdapterListener::OnDataFromClient(const std::string &clientId, const std::string &data)
{
   queue_->dispatch([this, clientId, data] {
      // Process all data on main thread (no need to worry about data races)
      processData(clientId, data);
   });
}

void SignerAdapterListener::OnClientConnected(const std::string &clientId)
{
   logger_->debug("[SignerAdapterListener] client {} connected", toHex(clientId));
}

void SignerAdapterListener::OnClientDisconnected(const std::string &clientId)
{
   logger_->debug("[SignerAdapterListener] client {} disconnected", toHex(clientId));

   shutdownIfNeeded();
}

void SignerAdapterListener::onClientError(const std::string &clientId, const std::string &error)
{
   logger_->debug("[SignerAdapterListener] client {} error: {}", toHex(clientId), error);

   shutdownIfNeeded();
}

void SignerAdapterListener::processData(const std::string &clientId, const std::string &data)
{
   signer::Packet packet;
   if (!packet.ParseFromString(data)) {
      logger_->error("[SignerAdapterListener::{}] failed to parse request packet", __func__);
      return;
   }
   bool rc = false;
   switch (packet.type()) {
   case::signer::HeadlessReadyType:
      rc = sendReady();
      break;
   case signer::SignOfflineTxRequestType:
      rc = onSignOfflineTxRequest(packet.data(), packet.id());
      break;
   case signer::SyncWalletInfoType:
      rc = onSyncWalletInfo(packet.id());
      break;
   case signer::SyncHDWalletType:
      rc = onSyncHDWallet(packet.data(), packet.id());
      break;
   case signer::SyncWalletType:
      rc = onSyncWallet(packet.data(), packet.id());
      break;
   case signer::CreateWOType:
   case signer::GetDecryptedNodeType:
      rc = onGetDecryptedNode(packet.data(), packet.id());
      break;
   case signer::SetLimitsType:
      rc = onSetLimits(packet.data());
      break;
   case signer::PasswordReceivedType:
      rc = onPasswordReceived(packet.data());
      break;
   case signer::RequestCloseType:
      rc = onRequestClose();
      break;
   case signer::ReloadWalletsType:
      rc = onReloadWallets(packet.data(), packet.id());
      break;
   case signer::AutoSignActType:
      rc = onAutoSignRequest(packet.data(), packet.id());
      break;
   case signer::ChangePasswordRequestType:
      rc = onChangePassword(packet.data(), packet.id());
      break;
   case signer::CreateHDWalletType:
      rc = onCreateHDWallet(packet.data(), packet.id());
      break;
   case signer::DeleteHDWalletType:
      rc = onDeleteHDWallet(packet.data(), packet.id());
      break;
   case signer::ImportWoWalletType:
      rc = onImportWoWallet(packet.data(), packet.id());
      break;
   case signer::ExportWoWalletType:
      rc = onExportWoWallet(packet.data(), packet.id());
      break;
   case signer::SyncSettingsRequestType:
      rc = onSyncSettings(packet.data());
      break;
   default:
      logger_->warn("[SignerAdapterListener::{}] unprocessed packet type {}", __func__, packet.type());
      break;
   }
   if (!rc) {
      sendData(packet.type(), "", packet.id());
   }
}

bool SignerAdapterListener::sendData(signer::PacketType pt, const std::string &data
   , bs::signer::RequestId reqId)
{
   if (!connection_) {
      return false;
   }

   signer::Packet packet;
   packet.set_type(pt);
   packet.set_data(data);
   if (reqId) {
      packet.set_id(reqId);
   }

   return connection_->SendDataToAllClients(packet.SerializeAsString());
}

void SignerAdapterListener::sendStatusUpdate()
{
   signer::UpdateStatus evt;
   evt.set_signer_bind_status(signer::BindStatus(app_->signerBindStatus()));
   evt.set_signer_pub_key(app_->signerPubKey().toBinStr());
   sendData(signer::UpdateStatusType, evt.SerializeAsString());

   callbacks_->ccNamesReceived(!walletsMgr_->ccLeaves().empty());
}

void SignerAdapterListener::resetConnection()
{
   connection_ = nullptr;
}

HeadlessContainerCallbacks *SignerAdapterListener::callbacks() const
{
   return callbacks_.get();
}

bool SignerAdapterListener::onSignOfflineTxRequest(const std::string &data, bs::signer::RequestId reqId)
{
   signer::SignOfflineTxRequest request;
   signer::SignTxEvent evt;

   if (!request.ParseFromString(data)) {
      logger_->error("[SignerAdapterListener::{}] failed to parse request", __func__);
      evt.set_errorcode((int)bs::error::ErrorCode::TxInvalidRequest);
      return sendData(signer::SignOfflineTxRequestType, evt.SerializeAsString(), reqId);;
   }
   const auto wallet = walletsMgr_->getWalletById(request.tx_request().wallet_id());
   if (!wallet) {
      logger_->error("[SignerAdapterListener::{}] failed to find wallet with id {}"
         , __func__, request.tx_request().wallet_id());
      evt.set_errorcode((int)bs::error::ErrorCode::WalletNotFound);
      return sendData(signer::SignOfflineTxRequestType, evt.SerializeAsString(), reqId);;
   }
   if (wallet->isWatchingOnly()) {
      logger_->error("[SignerAdapterListener::{}] can't sign with watching-only wallet {}"
         , __func__, request.tx_request().wallet_id());
      evt.set_errorcode((int)bs::error::ErrorCode::WalletNotFound);
      return sendData(signer::SignOfflineTxRequestType, evt.SerializeAsString(), reqId);;
   }
   bs::core::wallet::TXSignRequest txReq;
   txReq.walletId = request.tx_request().wallet_id();
   for (int i = 0; i < request.tx_request().inputs_size(); ++i) {
      UTXO utxo;
      utxo.unserialize(request.tx_request().inputs(i));
      txReq.inputs.emplace_back(std::move(utxo));
   }
   for (int i = 0; i < request.tx_request().recipients_size(); ++i) {
      const BinaryData bd(request.tx_request().recipients(i));
      txReq.recipients.push_back(ScriptRecipient::deserialize(bd));
   }
   txReq.fee = request.tx_request().fee();
   txReq.RBF = request.tx_request().rbf();
   if (request.tx_request().has_change()) {
      txReq.change.address = request.tx_request().change().address();
      txReq.change.index = request.tx_request().change().index();
      txReq.change.value = request.tx_request().change().value();
   }

   std::set<BinaryData> usedAddrSet;
   std::map<BinaryData, bs::hd::Path> parsedMap;
   for (const auto &utxo : txReq.inputs) {
      const auto addr = bs::Address::fromUTXO(utxo);
      usedAddrSet.insert(addr.id());
   }
   try {
      typedef std::map<bs::hd::Path, BinaryData> pathMapping;
      std::map<bs::hd::Path::Elem, pathMapping> mapByPath;
      parsedMap = wallet->indexPath(usedAddrSet);

      for (auto& parsedPair : parsedMap) {
         auto& mapping = mapByPath[parsedPair.second.get(-2)];
         mapping[parsedPair.second] = parsedPair.first;
      }

      unsigned int nbNewAddrs = 0;
      for (auto& mapping : mapByPath) {
         for (auto& pathPair : mapping.second) {
            const auto resultPair = wallet->synchronizeUsedAddressChain(
               pathPair.first.toString());
            if (resultPair.second) {
               nbNewAddrs++;
            }
         }
      }
      logger_->debug("[{}] created {} new address[es] after sync", __func__, nbNewAddrs);
   }
   catch (const AccountException &e) {
      logger_->error("[{}] failed to sync addresses: {}", __func__, e.what());
      evt.set_errorcode((int)bs::error::ErrorCode::WrongAddress);
      return sendData(signer::SignOfflineTxRequestType, evt.SerializeAsString(), reqId);;
   }

   try {
      auto lock = wallet->lockForEncryption(request.password());
      const auto tx = wallet->signTXRequest(txReq);
      evt.set_signedtx(tx.toBinStr());
      evt.set_errorcode((int)bs::error::ErrorCode::NoError);
      return sendData(signer::SignOfflineTxRequestType, evt.SerializeAsString(), reqId);
   }
   catch (const std::exception &e) {
      logger_->error("[SignerAdapterListener::{}] sign error: {}"
         , __func__, e.what());
   }
   evt.set_errorcode((int)bs::error::ErrorCode::InvalidPassword);
   return sendData(signer::SignOfflineTxRequestType, evt.SerializeAsString(), reqId);
}

bool SignerAdapterListener::onSyncWalletInfo(bs::signer::RequestId reqId)
{
   signer::SyncWalletInfoResponse response;
   for (size_t i = 0; i < walletsMgr_->getHDWalletsCount(); ++i) {
      auto wallet = response.add_wallets();
      const auto hdWallet = walletsMgr_->getHDWallet(i);
      wallet->set_format(signer::WalletFormatHD);
      wallet->set_id(hdWallet->walletId());
      wallet->set_name(hdWallet->name());
      wallet->set_description(hdWallet->description());
      wallet->set_watching_only(hdWallet->isWatchingOnly());
   }
   return sendData(signer::SyncWalletInfoType, response.SerializeAsString(), reqId);
}

bool SignerAdapterListener::onSyncHDWallet(const std::string &data, bs::signer::RequestId reqId)
{
   signer::SyncWalletRequest request;
   if (!request.ParseFromString(data)) {
      logger_->error("[SignerAdapterListener::{}] failed to parse request", __func__);
      return false;
   }
   const auto hdWallet = walletsMgr_->getHDWalletById(request.wallet_id());
   if (hdWallet) {
      signer::SyncHDWalletResponse response;
      for (const auto &group : hdWallet->getGroups()) {
         auto groupEntry = response.add_groups();
         groupEntry->set_type(static_cast<bs::hd::CoinType>(group->index()));

         for (const auto &leaf : group->getLeaves()) {
            auto leafEntry = groupEntry->add_leaves();
            leafEntry->set_id(leaf->walletId());
            leafEntry->set_path(leaf->path().toString());

            if (groupEntry->type() == bs::hd::CoinType::BlockSettle_Settlement) {
               const auto settlLeaf = std::dynamic_pointer_cast<bs::core::hd::SettlementLeaf>(leaf);
               if (settlLeaf == nullptr) {
                  throw std::runtime_error("unexpected leaf type");
               }
               const auto rootAsset = settlLeaf->getRootAsset();
               const auto rootSingle = std::dynamic_pointer_cast<AssetEntry_Single>(rootAsset);
               if (rootSingle == nullptr) {
                  throw std::runtime_error("invalid root asset");
               }
               const auto authAddr = BtcUtils::getHash160(rootSingle->getPubKey()->getCompressedKey());
               leafEntry->set_extra_data(authAddr.toBinStr());
            }
         }
      }
      return sendData(signer::SyncHDWalletType, response.SerializeAsString(), reqId);
   } else {
      logger_->error("[SignerAdapterListener::{}] failed to find HD wallet with id {}"
         , __func__, request.wallet_id());
   }
   return false;
}

bool SignerAdapterListener::onSyncWallet(const std::string &data, bs::signer::RequestId reqId)
{
   signer::SyncWalletRequest request;
   if (!request.ParseFromString(data)) {
      logger_->error("[SignerAdapterListener::{}] failed to parse request", __func__);
      return false;
   }
   const auto wallet = walletsMgr_->getWalletById(request.wallet_id());
   if (wallet) {
      signer::SyncWalletResponse response;
      response.set_wallet_id(wallet->walletId());
      for (const auto &encType : wallet->encryptionTypes()) {
         response.add_encryption_types(static_cast<signer::EncryptionType>(encType));
      }
      for (const auto &encKey : wallet->encryptionKeys()) {
         response.add_encryption_keys(encKey.toBinStr());
      }
      response.set_key_rank_m(wallet->encryptionRank().first);
      response.set_key_rank_n(wallet->encryptionRank().second);

      response.set_net_type(static_cast<int>(wallet->networkType()));
      response.set_highest_ext_index(wallet->getExtAddressCount());
      response.set_highest_int_index(wallet->getIntAddressCount());

      for (const auto &addr : wallet->getUsedAddressList()) {
         const auto index = wallet->getAddressIndex(addr);
         auto address = response.add_addresses();
         address->set_address(addr.display());
         address->set_index(index);
      }
      return sendData(signer::SyncWalletType, response.SerializeAsString(), reqId);
   }
   else {
      logger_->error("[SignerAdapterListener::{}] failed to find wallet with id {}"
         , __func__, request.wallet_id());
   }
   return false;
}

bool SignerAdapterListener::sendWoWallet(const std::shared_ptr<bs::core::hd::Wallet> &wallet
   , Blocksettle::Communication::signer::PacketType pt, bs::signer::RequestId reqId)
{
   signer::CreateWatchingOnlyResponse response;
   response.set_wallet_id(wallet->walletId());
   response.set_name(wallet->name());
   response.set_description(wallet->description());

   for (const auto &group : wallet->getGroups()) {
      auto groupEntry = response.add_groups();
      groupEntry->set_type(group->index());
      for (const auto &leaf : group->getLeaves()) {
         auto leafEntry = groupEntry->add_leaves();
         leafEntry->set_id(leaf->walletId());
         leafEntry->set_path(leaf->path().toString());
         for (const auto &addr : leaf->getUsedAddressList()) {
            auto addrEntry = leafEntry->add_addresses();
            addrEntry->set_index(leaf->getAddressIndex(addr));
            addrEntry->set_aet(addr.getType());
         }
      }
   }
   return sendData(pt, response.SerializeAsString(), reqId);
}

bool SignerAdapterListener::onGetDecryptedNode(const std::string &data, bs::signer::RequestId reqId)
{
   signer::DecryptWalletEvent request;
   if (!request.ParseFromString(data)) {
      logger_->error("[SignerAdapterListener::{}] failed to parse request", __func__);
      return false;
   }
   const auto hdWallet = walletsMgr_->getHDWalletById(request.wallet_id());
   if (!hdWallet) {
      logger_->error("[SignerAdapterListener::{}] failed to find HD wallet with id {}"
         , __func__, request.wallet_id());
      return false;
   }

   std::string seedStr, privKeyStr;

   try {
      const SecureBinaryData pwd(request.password());
      const auto lock = hdWallet->lockForEncryption(pwd);
      const auto &seed = hdWallet->getDecryptedSeed();
      seedStr = seed.seed().toBinStr();
      privKeyStr = seed.toXpriv().toBinStr();
   }
   catch (const WalletException &e) {
      logger_->error("[SignerAdapterListener::{}] failed to decrypt wallet with id {}"
         , __func__, request.wallet_id());
      return false;
   }

   signer::DecryptedNodeResponse response;
   response.set_wallet_id(hdWallet->walletId());
   response.set_private_key(privKeyStr);
   response.set_chain_code(seedStr);
   return sendData(signer::GetDecryptedNodeType, response.SerializeAsString(), reqId);
}

bool SignerAdapterListener::onSetLimits(const std::string &data)
{
   signer::SetLimitsRequest request;
   if (!request.ParseFromString(data)) {
      logger_->error("[SignerAdapterListener::{}] failed to parse request", __func__);
      return false;
   }
   bs::signer::Limits limits;
   limits.autoSignSpendXBT = request.auto_sign_satoshis();
   limits.manualSpendXBT = request.manual_satoshis();
   limits.autoSignTimeS = request.auto_sign_time();
   limits.manualPassKeepInMemS = request.password_keep_in_mem();
   app_->setLimits(limits);
   return true;
}

bool SignerAdapterListener::onSyncSettings(const std::string &data)
{
   signer::Settings settings;
   if (!settings.ParseFromString(data)) {
      logger_->error("[SignerAdapterListener::{}] failed to parse request", __func__);
      return false;
   }
   app_->updateSettings(settings);
   return true;
}

bool SignerAdapterListener::onPasswordReceived(const std::string &data)
{
   signer::DecryptWalletEvent request;
   if (!request.ParseFromString(data)) {
      logger_->error("[SignerAdapterListener::{}] failed to parse request", __func__);
      return false;
   }
   app_->passwordReceived(request.wallet_id(), static_cast<bs::error::ErrorCode>(request.errorcode()), request.password());
   return true;
}

bool SignerAdapterListener::onRequestClose()
{
   logger_->info("[SignerAdapterListener::{}] closing on interface request", __func__);
   app_->close();
   return true;
}

bool SignerAdapterListener::onReloadWallets(const std::string &data, bs::signer::RequestId reqId)
{
   signer::ReloadWalletsRequest request;
   if (!request.ParseFromString(data)) {
      logger_->error("[SignerAdapterListener::{}] failed to parse request", __func__);
      return false;
   }
   app_->reloadWallets(request.path(), [this, reqId] {
      sendData(signer::ReloadWalletsType, "", reqId);
   });
   return true;
}

bool SignerAdapterListener::onAutoSignRequest(const std::string &data, bs::signer::RequestId reqId)
{
   signer::AutoSignActRequest request;
   if (!request.ParseFromString(data)) {
      logger_->error("[SignerAdapterListener::{}] failed to parse request", __func__);
      return false;
   }

   bs::error::ErrorCode result = app_->activateAutoSign(request.rootwalletid(), request.activateautosign()
      , SecureBinaryData(request.password()));

   signer::AutoSignActResponse response;
   response.set_errorcode(static_cast<uint32_t>(result));
   response.set_rootwalletid(request.rootwalletid());
   response.set_autosignactive(request.activateautosign());

   sendData(signer::AutoSignActType, response.SerializeAsString(), reqId);

   return true;
}

bool SignerAdapterListener::onChangePassword(const std::string &data, bs::signer::RequestId reqId)
{
   signer::ChangePasswordResponse response;

   signer::ChangePasswordRequest request;
   if (!request.ParseFromString(data)) {
      logger_->error("[SignerContainerListener] failed to parse ChangePasswordRequest");
      response.set_success(false);
      response.set_rootwalletid(std::string());
      sendData(signer::ChangePasswordRequestType, response.SerializeAsString(), reqId);
      return false;
   }
   const auto &wallet = walletsMgr_->getHDWalletById(request.rootwalletid());
   if (!wallet) {
      logger_->error("[SignerContainerListener] failed to find wallet for id {}", request.rootwalletid());
      response.set_success(false);
      response.set_rootwalletid(request.rootwalletid());
      sendData(signer::ChangePasswordRequestType, response.SerializeAsString(), reqId);
      return false;
   }
   std::vector<bs::wallet::PasswordData> pwdData;
   for (int i = 0; i < request.newpassword_size(); ++i) {
      const auto &pwd = request.newpassword(i);
      pwdData.push_back({ SecureBinaryData(pwd.password())
                          , static_cast<bs::wallet::EncryptionType>(pwd.enctype()), pwd.enckey()});
   }
   bs::wallet::KeyRank keyRank = {request.rankm(), request.rankn()};

   bool result = wallet->changePassword(request.newpassword(0).password() /*pwdData, keyRank
                                        , BinaryData::CreateFromHex(request.oldpassword())
                                        , request.addnew(), request.removeold(), request.dryrun()*/);

   if (result) {
      walletsListUpdated();
   }

   response.set_success(result);
   response.set_rootwalletid(request.rootwalletid());
   logger_->info("[SignerAdapterListener::{}] password changed for wallet {} with result {}", __func__, request.rootwalletid(), result);
   return sendData(signer::ChangePasswordRequestType, response.SerializeAsString(), reqId);
}

static NetworkType mapNetworkType(Blocksettle::Communication::headless::NetworkType netType)
{
   switch (netType) {
   case Blocksettle::Communication::headless::MainNetType:   return NetworkType::MainNet;
   case Blocksettle::Communication::headless::TestNetType:   return NetworkType::TestNet;
   default:                      return NetworkType::Invalid;
   }
}

bool SignerAdapterListener::onCreateHDWallet(const std::string &data, bs::signer::RequestId reqId)
{
   headless::CreateHDWalletRequest request;
   if (!request.ParseFromString(data)) {
      return false;
   }

/*   std::vector<bs::wallet::PasswordData> pwdData;
   for (int i = 0; i < request.password_size(); ++i) {
      const auto pwd = request.password(i);
      pwdData.push_back({ pwd.password()
         , static_cast<bs::wallet::EncryptionType>(pwd.enctype()), pwd.enckey()});
   }
   bs::wallet::KeyRank keyRank = { request.rankm(), request.rankn() }; */

   std::shared_ptr<bs::core::hd::Wallet> wallet;
   try {
      const auto &w = request.wallet();
      const auto netType = mapNetworkType(w.nettype());
      const auto seed = w.privatekey().empty() ? bs::core::wallet::Seed(w.seed(), netType)
         : bs::core::wallet::Seed::fromXpriv(w.privatekey(), netType);
      const SecureBinaryData pwd(request.password(0).password());
      /*      auto seed = w.privatekey().empty() ? bs::core::wallet::Seed(w.seed(), netType)
         : bs::core::wallet::Seed(netType, w.privatekey(), w.chaincode());*/
      wallet = walletsMgr_->createWallet(w.name(), w.description()
         , seed, settings_->getWalletsDir(), pwd, w.primary()
      /*, pwdData, keyRank*/);

      walletsListUpdated();
   }
   catch (const std::exception &e) {
      logger_->error("[{}] failed to create HD Wallet: {}", __func__, e.what());
      headless::CreateHDWalletResponse response;
      response.set_errorcode(static_cast<uint32_t>(bs::error::ErrorCode::InternalError));
      return sendData(signer::CreateHDWalletType, response.SerializeAsString(), reqId);;
   }

   headless::CreateHDWalletResponse response;
   return sendData(signer::CreateHDWalletType, response.SerializeAsString(), reqId);
}

bool SignerAdapterListener::onDeleteHDWallet(const std::string &data, bs::signer::RequestId reqId)
{
   headless::DeleteHDWalletRequest request;
   if (!request.ParseFromString(data)) {
      return false;
   }

   const auto &walletId = request.rootwalletid();
   const auto &wallet = walletsMgr_->getHDWalletById(walletId);
   if (!wallet) {
      logger_->error("[{}] failed to find HD Wallet by id {}", __func__, walletId);

      headless::DeleteHDWalletResponse response;
      response.set_error(fmt::format("Can't find wallet {}", request.rootwalletid()));
      return sendData(signer::DeleteHDWalletType, response.SerializeAsString(), reqId);
   }

   logger_->debug("Deleting HDWallet {}: {}", walletId, wallet->name());

   bool result = walletsMgr_->deleteWalletFile(wallet);
   if (result) {
      walletsListUpdated();
   }

   headless::DeleteHDWalletResponse response;
   response.set_success(result);
   if (!result) {
      response.set_error("Unknown error");
   }

   return sendData(signer::DeleteHDWalletType, response.SerializeAsString(), reqId);
}

bool SignerAdapterListener::onImportWoWallet(const std::string &data, bs::signer::RequestId reqId)
{
   signer::ImportWoWalletRequest request;
   if (!request.ParseFromString(data)) {
      return false;
   }

   if (!SystemFileUtils::pathExist(settings_->getWalletsDir())) {
      if (SystemFileUtils::mkPath(settings_->getWalletsDir())) {
         logger_->info("[{}] created missing wallets dir {}", __func__, settings_->getWalletsDir());
      }
      else {
         logger_->error("[{}] failed to create wallets dir {}", __func__, settings_->getWalletsDir());
         return false;
      }
   }

   {
      const std::string filePath = settings_->getWalletsDir() + "/" + request.filename();
      std::ofstream ofs(filePath, std::ios::out | std::ios::binary | std::ios::trunc);
      if (!ofs.good()) {
         logger_->error("[{}] failed to write to {}", __func__, filePath);
         return false;
      }
      ofs << request.content();
   }

   const auto woWallet = walletsMgr_->loadWoWallet(settings_->netType()
      , settings_->getWalletsDir(), request.filename());
   if (!woWallet) {
      return false;
   }
   walletsListUpdated();
   return sendWoWallet(woWallet, signer::ImportWoWalletType, reqId);
}

bool SignerAdapterListener::onExportWoWallet(const std::string &data, bs::signer::RequestId reqId)
{
   signer::ExportWoWalletRequest request;
   if (!request.ParseFromString(data)) {
      return false;
   }

   auto woWallet = walletsMgr_->getHDWalletById(request.rootwalletid());
   if (!woWallet) {
      SPDLOG_LOGGER_ERROR(logger_, "can't find wallet with walletId: {}", request.rootwalletid());
      return false;
   }

   if (!woWallet->isWatchingOnly()) {
      SPDLOG_LOGGER_ERROR(logger_, "not a WO wallet: {}", request.rootwalletid());
      return false;
   }

   std::ifstream file(woWallet->getFileName(), std::ios::in | std::ios::binary | std::ios::ate);
   if (!file.is_open()) {
      SPDLOG_LOGGER_ERROR(logger_, "can't open WO file for read: {}", woWallet->getFileName());
      return false;
   }

   auto size = file.tellg();
   std::string content;
   content.resize(size_t(size));

   if (size == 0) {
      SPDLOG_LOGGER_ERROR(logger_, "empty WO file: {}", woWallet->getFileName());
      return false;
   }

   file.seekg(0, std::ios::beg);
   file.read(&content.front(), size);

   signer::ExportWoWalletResponse response;
   response.set_content(std::move(content));
   return sendData(signer::ExportWoWalletType, response.SerializeAsString(), reqId);
}

void SignerAdapterListener::walletsListUpdated()
{
   logger_->debug("[{}]", __func__);
   app_->walletsListUpdated();
   sendData(signer::WalletsListUpdatedType, {});
}

void SignerAdapterListener::shutdownIfNeeded()
{
   if (settings_->runMode() == bs::signer::RunMode::litegui && app_) {
      logger_->info("terminal disconnect detected, shutdown...");
      app_->close();
   }
}

bool SignerAdapterListener::sendReady()
{
   // Notify GUI about bind status
   sendStatusUpdate();

   return sendData(signer::HeadlessReadyType, {});
}
