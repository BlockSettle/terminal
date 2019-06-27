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
   {
   }

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
      if (owner_->settings_->runMode() == bs::signer::RunMode::lightgui) {
         owner_->logger_->info("Quit because terminal disconnected unexpectedly and lightgui used");
         owner_->queue_->quit();
      }
   }

   void pwd(const bs::core::wallet::TXSignRequest &txReq, const std::string &prompt) override
   {
      signer::PasswordEvent evt;
      evt.set_wallet_id(txReq.walletId);
      evt.set_prompt(prompt);
      if (txReq.autoSign) {
         evt.set_auto_sign(true);
      } else {
         for (const auto &input : txReq.inputs) {
            evt.add_inputs(input.serialize().toBinStr());
         }
         for (const auto &recip : txReq.recipients) {
            evt.add_recipients(recip->getSerializedScript().toBinStr());
         }
         evt.set_fee(txReq.fee);
         evt.set_rbf(txReq.RBF);
         if (txReq.change.value) {
            auto change = evt.mutable_change();
            change->set_address(txReq.change.address.display());
            change->set_index(txReq.change.index);
            change->set_value(txReq.change.value);
         }
      }
      owner_->sendData(signer::PasswordRequestType, evt.SerializeAsString());
   }

   void txSigned(const BinaryData &tx) override
   {
      signer::TxSignEvent evt;
      evt.set_tx(tx.toBinStr());
      owner_->sendData(signer::TxSignedType, evt.SerializeAsString());
   }

   void cancelTxSign(const BinaryData &txHash) override
   {
      signer::TxSignEvent evt;
      evt.set_tx_hash(txHash.toBinStr());
      owner_->sendData(signer::CancelTxSignType, evt.SerializeAsString());
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
      signer::TerminalHandshakeFailed evt;
      evt.set_peeraddress(peerAddress);
      owner_->sendData(signer::TerminalHandshakeFailedType, evt.SerializeAsString());
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
   case signer::SignTxRequestType:
      rc = onSignTxReq(packet.data(), packet.id());
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
      rc = onCreateWO(packet.data(), packet.id());
      break;
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
   case signer::HeadlessPubKeyRequestType:
      rc = onHeadlessPubKeyRequest(packet.data(), packet.id());
      break;
   case signer::ImportWoWalletType:
      rc = onImportWoWallet(packet.data(), packet.id());
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
   sendData(signer::UpdateStatusType, evt.SerializeAsString());
}

void SignerAdapterListener::resetConnection()
{
   connection_ = nullptr;
}

HeadlessContainerCallbacks *SignerAdapterListener::callbacks() const
{
   return callbacks_.get();
}

bool SignerAdapterListener::onSignTxReq(const std::string &data, bs::signer::RequestId reqId)
{
   signer::SignTxRequest request;
   if (!request.ParseFromString(data)) {
      logger_->error("[SignerAdapterListener::{}] failed to parse request", __func__);
      return false;
   }
   const auto wallet = walletsMgr_->getWalletById(request.tx_request().wallet_id());
   if (!wallet) {
      logger_->error("[SignerAdapterListener::{}] failed to find wallet with id {}"
         , __func__, request.tx_request().wallet_id());
      return false;
   }
   if (wallet->isWatchingOnly()) {
      logger_->error("[SignerAdapterListener::{}] can't sign with watching-only wallet {}"
         , __func__, request.tx_request().wallet_id());
      return false;
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

   try {
      const auto tx = wallet->signTXRequest(txReq, request.password());
      signer::TxSignEvent evt;
      evt.set_tx(tx.toBinStr());
      return sendData(signer::SignTxRequestType, evt.SerializeAsString(), reqId);
   }
   catch (const std::exception &e) {
      logger_->error("[SignerAdapterListener::{}] sign error: {}"
         , __func__, e.what());
   }
   return false;
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
   const auto settlWallet = walletsMgr_->getSettlementWallet();
   if (settlWallet) {
      auto wallet = response.add_wallets();
      wallet->set_format(signer::WalletFormatSettlement);
      wallet->set_id(settlWallet->walletId());
      wallet->set_name(settlWallet->name());
      wallet->set_description(settlWallet->description());
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
            leafEntry->set_index(leaf->index());
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
         leafEntry->set_index(leaf->index());
         leafEntry->set_public_key(leaf->getPubKey().toBinStr());
         for (const auto &addr : leaf->getUsedAddressList()) {
            auto addrEntry = leafEntry->add_addresses();
            addrEntry->set_index(leaf->getAddressIndex(addr));
            addrEntry->set_aet(addr.getType());
         }
      }
   }
   return sendData(pt, response.SerializeAsString(), reqId);
}

bool SignerAdapterListener::onCreateWO(const std::string &data, bs::signer::RequestId reqId)
{
   signer::DecryptWalletRequest request;
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
   const auto woWallet = hdWallet->createWatchingOnly(request.password());
   if (!woWallet) {
      logger_->error("[SignerAdapterListener::{}] failed to create watching-only wallet for id {}"
         , __func__, request.wallet_id());
      return false;
   }

   return sendWoWallet(woWallet, signer::CreateWOType, reqId);
}

bool SignerAdapterListener::onGetDecryptedNode(const std::string &data, bs::signer::RequestId reqId)
{
   signer::DecryptWalletRequest request;
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
   const auto decrypted = hdWallet->getRootNode(request.password());
   if (!decrypted) {
      logger_->error("[SignerAdapterListener::{}] failed to decrypt wallet with id {}"
         , __func__, request.wallet_id());
      return false;
   }

   signer::DecryptedNodeResponse response;
   response.set_wallet_id(hdWallet->walletId());
   response.set_private_key(decrypted->privateKey().toBinStr());
   response.set_chain_code(decrypted->chainCode().toBinStr());
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
   signer::DecryptWalletRequest request;
   if (!request.ParseFromString(data)) {
      logger_->error("[SignerAdapterListener::{}] failed to parse request", __func__);
      return false;
   }
   app_->passwordReceived(request.wallet_id(), request.password(), request.cancelled_by_user());
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
      pwdData.push_back({ BinaryData::CreateFromHex(pwd.password())
                          , static_cast<bs::wallet::EncryptionType>(pwd.enctype()), pwd.enckey()});
   }
   bs::wallet::KeyRank keyRank = {request.rankm(), request.rankn()};

   bool result = wallet->changePassword(pwdData, keyRank
                                        , BinaryData::CreateFromHex(request.oldpassword())
                                        , request.addnew(), request.removeold(), request.dryrun());

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

   std::vector<bs::wallet::PasswordData> pwdData;
   for (int i = 0; i < request.password_size(); ++i) {
      const auto pwd = request.password(i);
      pwdData.push_back({BinaryData::CreateFromHex(pwd.password())
         , static_cast<bs::wallet::EncryptionType>(pwd.enctype()), pwd.enckey()});
   }
   bs::wallet::KeyRank keyRank = { request.rankm(), request.rankn() };

   std::shared_ptr<bs::core::hd::Wallet> wallet;
   try {
      const auto &w = request.wallet();
      auto netType = mapNetworkType(w.nettype());
      auto seed = w.privatekey().empty() ? bs::core::wallet::Seed(w.seed(), netType)
         : bs::core::wallet::Seed(netType, w.privatekey(), w.chaincode());
      wallet = walletsMgr_->createWallet(w.name(), w.description()
         , seed, settings_->getWalletsDir(), w.primary(), pwdData, keyRank);

      walletsListUpdated();
   }
   catch (const std::exception &e) {
      headless::CreateHDWalletResponse response;
      response.set_error(e.what());
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

bool SignerAdapterListener::onHeadlessPubKeyRequest(const std::string &, bs::signer::RequestId reqId)
{
   signer::HeadlessPubKeyResponse response;
   if (app_ && app_->connection()) {
      response.set_pubkey(app_->connection()->getOwnPubKey().toHexStr());
   }

   return sendData(signer::HeadlessPubKeyRequestType, response.SerializeAsString(), reqId);
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

   const auto woWallet = walletsMgr_->loadWoWallet(settings_->getWalletsDir(), request.filename());
   if (!woWallet) {
      return false;
   }
   walletsListUpdated();
   return sendWoWallet(woWallet, signer::ImportWoWalletType, reqId);
}

void SignerAdapterListener::walletsListUpdated()
{
   logger_->debug("[{}]", __func__);
   app_->walletsListUpdated();
   sendData(signer::WalletsListUpdatedType, {});
}

void SignerAdapterListener::shutdownIfNeeded()
{
   if (settings_->runMode() == bs::signer::RunMode::lightgui && app_) {
      logger_->info("terminal disconnect detected, shutdown...");
      app_->close();
   }
}

bool SignerAdapterListener::sendReady()
{
   return sendData(signer::HeadlessReadyType, {});
}
