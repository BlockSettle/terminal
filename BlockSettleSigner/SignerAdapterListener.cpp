#include "SignerAdapterListener.h"
#include <spdlog/spdlog.h>
#include "CoreHDWallet.h"
#include "CoreWalletsManager.h"
#include "HeadlessApp.h"
#include "HeadlessContainerListener.h"
#include "ServerConnection.h"

using namespace Blocksettle::Communication;

static std::string toHex(const std::string &binData)
{
   return BinaryData(binData).toHexStr();
}

SignerAdapterListener::SignerAdapterListener(HeadlessAppObj *app
   , const std::shared_ptr<ServerConnection> &conn
   , const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<bs::core::WalletsManager> &walletsMgr)
   : ServerConnectionListener(), app_(app)
   , connection_(conn), logger_(logger), walletsMgr_(walletsMgr)
{
   app_->setReadyCallback([this](bool result) {
      ready_ = result;
      if (result) {
         setCallbacks();
      }
      onReady();
   });
}

SignerAdapterListener::~SignerAdapterListener() = default;

void SignerAdapterListener::OnDataFromClient(const std::string &clientId, const std::string &data)
{
   signer::Packet packet;
   if (!packet.ParseFromString(data)) {
      logger_->error("[SignerAdapterListener::{}] failed to parse request packet", __func__);
      return;
   }
   bool rc = false;
   switch (packet.type()) {
   case::signer::HeadlessReadyType:
      rc = onReady();
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
   case signer::ReconnectTerminalType:
      rc = onReconnect(packet.data());
      break;
   case signer::AutoSignActType:
      rc = onAutoSignRequest(packet.data());
      break;
   default:
      logger_->warn("[SignerAdapterListener::{}] unprocessed packet type {}", __func__, packet.type());
      break;
   }
   if (!rc) {
      sendData(packet.type(), "", packet.id());
   }
}

void SignerAdapterListener::OnClientConnected(const std::string &clientId)
{
   logger_->debug("[SignerAdapterListener] client {} connected", toHex(clientId));
}

void SignerAdapterListener::OnClientDisconnected(const std::string &clientId)
{
   logger_->debug("[SignerAdapterListener] client {} disconnected", toHex(clientId));
}

void SignerAdapterListener::setCallbacks()
{
   const auto &cbPeerConnected = [this](const std::string &ip) {
      signer::PeerEvent evt;
      evt.set_ip_address(ip);
      sendData(signer::PeerConnectedType, evt.SerializeAsString());
   };
   const auto &cbPeerDisconnected = [this](const std::string &ip) {
      signer::PeerEvent evt;
      evt.set_ip_address(ip);
      sendData(signer::PeerDisconnectedType, evt.SerializeAsString());
   };
   const auto &cbPwd = [this](const bs::core::wallet::TXSignRequest &txReq, const std::string &prompt) {
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
            change->set_address(txReq.change.address.display<std::string>());
            change->set_index(txReq.change.index);
            change->set_value(txReq.change.value);
         }
      }
      sendData(signer::PasswordRequestType, evt.SerializeAsString());
   };
   const auto &cbTxSigned = [this](const BinaryData &tx) {
      signer::TxSignEvent evt;
      evt.set_tx(tx.toBinStr());
      sendData(signer::TxSignedType, evt.SerializeAsString());
   };
   const auto &cbCancelTxSign = [this](const BinaryData &txHash) {
      signer::TxSignEvent evt;
      evt.set_tx_hash(txHash.toBinStr());
      sendData(signer::CancelTxSignType, evt.SerializeAsString());
   };
   const auto &cbXbtSpent = [this](const int64_t value, bool autoSign) {
      signer::XbtSpentEvent evt;
      evt.set_value(value);
      evt.set_auto_sign(autoSign);
      sendData(signer::XbtSpentType, evt.SerializeAsString());
   };
   const auto &cbAutoSign = [this](bool act, const std::string &walletId) {
      signer::AutoSignActEvent evt;
      evt.set_activated(act);
      evt.set_wallet_id(walletId);
      sendData(signer::AutoSignActType, evt.SerializeAsString());
   };
   const auto &cbAutoSignActivated = [this, cbAutoSign](const std::string &walletId) {
      cbAutoSign(true, walletId);
   };
   const auto &cbAutoSignDeactivated = [this, cbAutoSign](const std::string &walletId) {
      cbAutoSign(false, walletId);
   };
   app_->setCallbacks(cbPeerConnected, cbPeerDisconnected, cbPwd, cbTxSigned, cbCancelTxSign
      , cbXbtSpent, cbAutoSignActivated, cbAutoSignDeactivated);
}

bool SignerAdapterListener::sendData(signer::PacketType pt, const std::string &data
   , SignContainer::RequestId reqId)
{
   signer::Packet packet;
   packet.set_type(pt);
   packet.set_data(data);
   if (reqId) {
      packet.set_id(reqId);
   }

   return connection_->SendDataToAllClients(packet.SerializeAsString());
}

bool SignerAdapterListener::onReady(int cur, int total)
{
   signer::ReadyEvent evt;
   evt.set_ready(ready_);
   evt.set_cur_wallet(cur);
   evt.set_total_wallets(total);
   sendData(signer::HeadlessReadyType, evt.SerializeAsString());
   return true;
}

bool SignerAdapterListener::onSignTxReq(const std::string &data, SignContainer::RequestId reqId)
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

bool SignerAdapterListener::onSyncWalletInfo(SignContainer::RequestId reqId)
{
   signer::SyncWalletInfoResponse response;
   for (size_t i = 0; i < walletsMgr_->getHDWalletsCount(); ++i) {
      auto wallet = response.add_wallets();
      const auto hdWallet = walletsMgr_->getHDWallet(i);
      wallet->set_format(signer::WalletFormatHD);
      wallet->set_id(hdWallet->walletId());
      wallet->set_name(hdWallet->name());
      wallet->set_description(hdWallet->description());
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

bool SignerAdapterListener::onSyncHDWallet(const std::string &data, SignContainer::RequestId reqId)
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

bool SignerAdapterListener::onSyncWallet(const std::string &data, SignContainer::RequestId reqId)
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
         address->set_address(addr.display<std::string>());
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

bool SignerAdapterListener::onCreateWO(const std::string &data, SignContainer::RequestId reqId)
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

   signer::CreateWatchingOnlyResponse response;
   response.set_wallet_id(woWallet->walletId());
   response.set_name(woWallet->name());
   response.set_description(woWallet->description());

   for (const auto &group : woWallet->getGroups()) {
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
   return sendData(signer::CreateWOType, response.SerializeAsString(), reqId);
}

bool SignerAdapterListener::onGetDecryptedNode(const std::string &data, SignContainer::RequestId reqId)
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
   SignContainer::Limits limits;
   limits.autoSignSpendXBT = request.auto_sign_satoshis();
   limits.manualSpendXBT = request.manual_satoshis();
   limits.autoSignTimeS = request.auto_sign_time();
   limits.manualPassKeepInMemS = request.password_keep_in_mem();
   app_->setLimits(limits);
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

bool SignerAdapterListener::onReloadWallets(const std::string &data, SignContainer::RequestId reqId)
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

bool SignerAdapterListener::onReconnect(const std::string &data)
{
   signer::ReconnectRequest request;
   if (!request.ParseFromString(data)) {
      logger_->error("[SignerAdapterListener::{}] failed to parse request", __func__);
      return false;
   }
   if (request.listen_address().empty() || request.listen_port().empty()) {
      app_->setOnline(request.online());
   }
   else {
      app_->reconnect(request.listen_address(), request.listen_port());
   }
   return true;
}

bool SignerAdapterListener::onAutoSignRequest(const std::string &data)
{
   signer::AutoSignActEvent request;
   if (!request.ParseFromString(data)) {
      logger_->error("[SignerAdapterListener::{}] failed to parse request", __func__);
      return false;
   }
   if (request.activated() && !request.wallet_id().empty()) {
      app_->addPendingAutoSignReq(request.wallet_id());
   }
   else {
      app_->deactivateAutoSign();
   }
   return true;
}
