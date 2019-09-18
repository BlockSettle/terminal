#include "HeadlessContainerListener.h"

#include "AuthAddressLogic.h"
#include "CheckRecipSigner.h"
#include "ConnectionManager.h"
#include "CoreHDWallet.h"
#include "CoreWalletsManager.h"
#include "DispatchQueue.h"
#include "ProtobufHeadlessUtils.h"
#include "ServerConnection.h"
#include "StringUtils.h"
#include "WalletEncryption.h"
#include "ZmqHelperFunctions.h"

#include <spdlog/spdlog.h>

using namespace Blocksettle::Communication;
using namespace bs::error;
using namespace std::chrono;

constexpr std::chrono::seconds kDefaultDuration{60};

HeadlessContainerListener::HeadlessContainerListener(const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<bs::core::WalletsManager> &walletsMgr
   , const std::shared_ptr<DispatchQueue> &queue
   , const std::string &walletsPath, NetworkType netType
   , const bool &backupEnabled)
   : ServerConnectionListener()
   , logger_(logger)
   , walletsMgr_(walletsMgr)
   , queue_(queue)
   , walletsPath_(walletsPath)
   , backupPath_(walletsPath + "/../backup")
   , netType_(netType)
   , backupEnabled_(backupEnabled)
{}

void HeadlessContainerListener::setCallbacks(HeadlessContainerCallbacks *callbacks)
{
   callbacks_ = callbacks;
}

HeadlessContainerListener::~HeadlessContainerListener() noexcept
{
   disconnect();
}

bool HeadlessContainerListener::disconnect(const std::string &clientId)
{
   headless::RequestPacket packet;
   packet.set_data("");
   packet.set_type(headless::DisconnectionRequestType);
   const auto &serializedPkt = packet.SerializeAsString();

   bool rc = sendData(serializedPkt, clientId);
   if (rc && !clientId.empty()) {
      OnClientDisconnected(clientId);
   }
   return rc;
}

bool HeadlessContainerListener::sendData(const std::string &data, const std::string &clientId)
{
   if (!connection_) {
      return false;
   }

   bool sentOk = false;
   if (clientId.empty()) {
      for (const auto &clientId : connectedClients_) {
         if (connection_->SendDataToClient(clientId, data)) {
            sentOk = true;
         }
      }
   }
   else {
      sentOk = connection_->SendDataToClient(clientId, data);
   }
   return sentOk;
}

void HeadlessContainerListener::SetLimits(const bs::signer::Limits &limits)
{
   limits_ = limits;
}

void HeadlessContainerListener::OnClientConnected(const std::string &clientId)
{
   logger_->debug("[HeadlessContainerListener] client {} connected", bs::toHex(clientId));

   queue_->dispatch([this, clientId] {
      connectedClients_.insert(clientId);
   });
}

void HeadlessContainerListener::OnClientDisconnected(const std::string &clientId)
{
   logger_->debug("[HeadlessContainerListener] client {} disconnected", bs::toHex(clientId));

   queue_->dispatch([this, clientId] {
      connectedClients_.erase(clientId);

      if (callbacks_) {
         callbacks_->clientDisconn(clientId);
      }
   });
}

void HeadlessContainerListener::OnDataFromClient(const std::string &clientId, const std::string &data)
{
   queue_->dispatch([this, clientId, data] {
      headless::RequestPacket packet;
      if (!packet.ParseFromString(data)) {
         logger_->error("[{}] failed to parse request packet", __func__);
         return;
      }

      onRequestPacket(clientId, packet);
   });
}

void HeadlessContainerListener::OnPeerConnected(const std::string &ip)
{
   logger_->debug("[{}] IP {} connected", __func__, ip);
   queue_->dispatch([this, ip] {
      if (callbacks_) {
         callbacks_->peerConn(ip);
      }
   });
}

void HeadlessContainerListener::OnPeerDisconnected(const std::string &ip)
{
   logger_->debug("[{}] IP {} disconnected", __func__, ip);
   queue_->dispatch([this, ip] {
      if (callbacks_) {
         callbacks_->peerDisconn(ip);
      }
   });
}

void HeadlessContainerListener::onClientError(const std::string &clientId, ServerConnectionListener::ClientError errorCode, int socket)
{

   switch (errorCode) {
      case ServerConnectionListener::HandshakeFailed: {
         // Not 100% correct because socket's FD might be already closed or even reused, but should be good enough
         std::string peerAddress = bs::network::peerAddressString(socket);
         queue_->dispatch([this, peerAddress] {
            if (callbacks_) {
               callbacks_->terminalHandshakeFailed(peerAddress);
            }
         });
         break;
      }
      default:
         break;
   }
}

bool HeadlessContainerListener::onRequestPacket(const std::string &clientId, headless::RequestPacket packet)
{
   if (!connection_) {
      logger_->error("[HeadlessContainerListener::{}] connection_ is not set");
      return false;
   }

   connection_->GetClientInfo(clientId);
   switch (packet.type()) {
   case headless::AuthenticationRequestType:
      return AuthResponse(clientId, packet);

   case headless::CancelSignTxRequestType:
      return onCancelSignTx(clientId, packet);

   case headless::UpdateDialogDataType:
      return onUpdateDialogData(clientId, packet);

   case headless::SignTxRequestType:
   case headless::SignSettlementTxRequestType:
   case headless::SignPartialTXRequestType:
   case headless::SignSettlementPartialTxType:
      return onSignTxRequest(clientId, packet, packet.type());

   case headless::SignSettlementPayoutTxType:
      return onSignSettlementPayoutTxRequest(clientId, packet);

   case headless::SignTXMultiRequestType:
      return onSignMultiTXRequest(clientId, packet);

   case headless::SignAuthAddrRevokeType:
      return onSignAuthAddrRevokeRequest(clientId, packet);

   case headless::CreateHDLeafRequestType:
      return onCreateHDLeaf(clientId, packet);

   case headless::PromoteHDWalletRequestType:
      return onPromoteHDWallet(clientId, packet);

   case headless::SetUserIdType:
      return onSetUserId(clientId, packet);

   case headless::SyncCCNamesType:
      return onSyncCCNames(packet);

   case headless::CreateSettlWalletType:
      return onCreateSettlWallet(clientId, packet);

   case headless::SetSettlementIdType:
      return onSetSettlementId(clientId, packet);

   case headless::GetSettlPayinAddrType:
      return onGetPayinAddr(clientId, packet);

   case headless::SettlGetRootPubkeyType:
      return onSettlGetRootPubkey(clientId, packet);

   case headless::GetHDWalletInfoRequestType:
      return onGetHDWalletInfo(clientId, packet);

   case headless::DisconnectionRequestType:
      break;

   case headless::SyncWalletInfoType:
      return onSyncWalletInfo(clientId, packet);

   case headless::SyncHDWalletType:
      return onSyncHDWallet(clientId, packet);

   case headless::SyncWalletType:
      return onSyncWallet(clientId, packet);

   case headless::SyncCommentType:
      return onSyncComment(clientId, packet);

   case headless::SyncAddressesType:
      return onSyncAddresses(clientId, packet);

   case headless::ExtendAddressChainType:
      return onExtAddrChain(clientId, packet);

   case headless::SyncNewAddressType:
      return onSyncNewAddr(clientId, packet);

   case headless::ExecCustomDialogRequestType:
      return onExecCustomDialog(clientId, packet);

   default:
      logger_->error("[HeadlessContainerListener] unknown request type {}", packet.type());
      return false;
   }
   return true;
}

bool HeadlessContainerListener::AuthResponse(const std::string &clientId, headless::RequestPacket packet)
{
   headless::AuthenticationReply response;
   response.set_authticket("");  // no auth tickets after moving to BIP150/151
   response.set_nettype((netType_ == NetworkType::TestNet) ? headless::TestNetType : headless::MainNetType);

   packet.set_data(response.SerializeAsString());
   return sendData(packet.SerializeAsString(), clientId);
}

bool HeadlessContainerListener::onSignTxRequest(const std::string &clientId, const headless::RequestPacket &packet
   , headless::RequestType reqType)
{
   bool partial = (reqType == headless::RequestType::SignPartialTXRequestType)
       || (reqType == headless::RequestType::SignSettlementPartialTxType);

   headless::SignTxRequest request;
   Internal::PasswordDialogDataWrapper dialogData;

   if (reqType == headless::RequestType::SignSettlementTxRequestType
       || reqType == headless::RequestType::SignSettlementPartialTxType){

      headless::SignSettlementTxRequest settlementRequest;

      if (!settlementRequest.ParseFromString(packet.data())) {
         logger_->error("[HeadlessContainerListener] failed to parse SignTxRequest");
         SignTXResponse(clientId, packet.id(), reqType, ErrorCode::FailedToParse);
         return false;
      }

      request = settlementRequest.signtxrequest();
      dialogData = settlementRequest.passworddialogdata();
   }
   else {
      if (!request.ParseFromString(packet.data())) {
         logger_->error("[HeadlessContainerListener] failed to parse SignTxRequest");
         SignTXResponse(clientId, packet.id(), reqType, ErrorCode::FailedToParse);
         return false;
      }
   }

   bs::core::wallet::TXSignRequest txSignReq = bs::signer::pbTxRequestToCore(request);

   if (!txSignReq.isValid()) {
      logger_->error("[HeadlessContainerListener] invalid SignTxRequest");
      SignTXResponse(clientId, packet.id(), reqType, ErrorCode::TxInvalidRequest);
      return false;
   }

   std::vector<std::shared_ptr<bs::core::hd::Leaf>> wallets;
   std::string rootWalletId;
   uint64_t amount = 0;

   for (const auto &walletId : txSignReq.walletIds) {
      const auto wallet = walletsMgr_->getWalletById(walletId);
      if (!wallet) {
         logger_->error("[HeadlessContainerListener] failed to find wallet {}", walletId);
         SignTXResponse(clientId, packet.id(), reqType, ErrorCode::WalletNotFound);
         return false;
      }
      wallets.push_back(wallet);
      const auto curRootWalletId = walletsMgr_->getHDRootForLeaf(walletId)->walletId();
      if (!rootWalletId.empty() && (rootWalletId != curRootWalletId)) {
         logger_->error("[HeadlessContainerListener] can't sign leaves from many roots");
         SignTXResponse(clientId, packet.id(), reqType, ErrorCode::WalletNotFound);
         return false;
      }
      rootWalletId = curRootWalletId;

      // get total spent
      const std::function<bool(const bs::Address &)> &containsAddressCb = [this, walletId](const bs::Address &address){
         const auto &hdWallet = walletsMgr_->getHDWalletById(walletId);
         if (hdWallet) {
            for (auto leaf : hdWallet->getLeaves()) {
               if (leaf->containsAddress(address)) {
                  return true;
               }
            }
         }
         else {
            const auto &wallet = walletsMgr_->getWalletById(walletId);
            if (wallet) {
               return wallet->containsAddress(address);
            }
         }
         return false;
      };

      if ((wallet->type() == bs::core::wallet::Type::Bitcoin)) {
         amount += txSignReq.amount(containsAddressCb);
      }

   }

   // FIXME: review spend limits
   if (!CheckSpendLimit(amount, rootWalletId)) {
      SignTXResponse(clientId, packet.id(), reqType, ErrorCode::TxSpendLimitExceed);
      return false;
   }

   if (wallets.empty()) {
      logger_->error("[HeadlessContainerListener] failed to find any wallets");
      SignTXResponse(clientId, packet.id(), reqType, ErrorCode::WalletNotFound);
      return false;
   }

   const auto onPassword = [this, wallets, txSignReq, rootWalletId, clientId, id = packet.id(), partial
      , reqType, amount
      , keepDuplicatedRecipients = request.keepduplicatedrecipients()]
      (bs::error::ErrorCode result, const SecureBinaryData &pass)
   {
      if (result == ErrorCode::TxCanceled) {
         logger_->error("[HeadlessContainerListener] transaction cancelled for wallet {}", wallets.front()->name());
         SignTXResponse(clientId, id, reqType, result);
         return;
      }

      // check spend limits one more time after password received
      if (!CheckSpendLimit(amount, rootWalletId)) {
         SignTXResponse(clientId, id, reqType, ErrorCode::TxSpendLimitExceed);
         return;
      }

      try {
         if (!wallets.front()->encryptionTypes().empty() && pass.isNull()) {
            logger_->error("[HeadlessContainerListener] empty password for wallet {}", wallets.front()->name());
            SignTXResponse(clientId, id, reqType, ErrorCode::MissingPassword);
            return;
         }
         if (wallets.size() == 1) {
            const auto wallet = wallets.front();
            auto passLock = wallet->lockForEncryption(pass);
            const auto tx = partial ? wallet->signPartialTXRequest(txSignReq)
               : wallet->signTXRequest(txSignReq, keepDuplicatedRecipients);
            SignTXResponse(clientId, id, reqType, ErrorCode::NoError, tx);
         }
         else {
            bs::core::wallet::TXMultiSignRequest multiReq;
            multiReq.recipients = txSignReq.recipients;
            if (txSignReq.change.value) {
               multiReq.recipients.push_back(txSignReq.change.address.getRecipient(txSignReq.change.value));
            }
            if (!txSignReq.prevStates.empty()) {
               multiReq.prevState = txSignReq.prevStates.front();
            }
            multiReq.RBF = txSignReq.RBF;

            bs::core::WalletMap wallets;
            for (const auto &input : txSignReq.inputs) {
               const auto addr = bs::Address::fromUTXO(input);
               const auto wallet = walletsMgr_->getWalletByAddress(addr);
               if (!wallet) {
                  logger_->error("[{}] failed to find wallet for input address {}"
                     , __func__, addr.display());
                  SignTXResponse(clientId, id, reqType, ErrorCode::WalletNotFound);
                  return;
               }
               multiReq.addInput(input, wallet->walletId());
               wallets[wallet->walletId()] = wallet;
            }

            const auto hdWallet = walletsMgr_->getHDWalletById(rootWalletId);
            BinaryData tx;
            {
               auto lock = hdWallet->lockForEncryption(pass);
               tx = bs::core::SignMultiInputTX(multiReq, wallets);
            }
            SignTXResponse(clientId, id, reqType, ErrorCode::NoError, tx);
         }

         onXbtSpent(amount, isAutoSignActive(rootWalletId));
         if (callbacks_) {
            callbacks_->xbtSpent(amount, false);
         }
      }
      catch (const std::exception &e) {
         logger_->error("[HeadlessContainerListener] failed to sign {} TX request: {}", partial ? "partial" : "full", e.what());
         SignTXResponse(clientId, id, reqType, ErrorCode::InternalError);
         passwords_.erase(rootWalletId);
      }
   };

   logger_->debug("[{}] rootWalletId={}", __func__, rootWalletId);
   dialogData.insert("WalletId", rootWalletId);
   return RequestPasswordIfNeeded(clientId, rootWalletId, txSignReq, reqType, dialogData, onPassword);
}

bool HeadlessContainerListener::onCancelSignTx(const std::string &, headless::RequestPacket packet)
{
   headless::CancelSignTx request;
   if (!request.ParseFromString(packet.data())) {
      logger_->error("[HeadlessContainerListener] failed to parse CancelSignTx");
      return false;
   }

   if (callbacks_) {
      callbacks_->cancelTxSign(request.tx_id());
   }

   return true;
}

bool HeadlessContainerListener::onUpdateDialogData(const std::string &clientId, headless::RequestPacket packet)
{
   headless::UpdateDialogDataRequest request;

   if (!request.ParseFromString(packet.data())) {
      logger_->error("[{}] failed to parse request", __func__);
      return false;
   }

   // try to find dialog in queued dialogs deferredPasswordRequests_
   auto it = deferredPasswordRequests_.begin();
   while (it != deferredPasswordRequests_.end()) {
      Internal::PasswordDialogDataWrapper otherDialogData = request.passworddialogdata();
      const auto &id = otherDialogData.value<std::string>("SettlementId");
      if (!id.empty() && it->dialogData.value<std::string>("SettlementId") == id) {
         it->dialogData.MergeFrom(request.passworddialogdata());
      }

      it++;
   }

   if (callbacks_) {
      callbacks_->updateDialogData(request.passworddialogdata());
   }
   return true;
}

bool HeadlessContainerListener::onSignMultiTXRequest(const std::string &clientId, const headless::RequestPacket &packet)
{
   const auto reqType = headless::SignTXMultiRequestType;
   headless::SignTXMultiRequest request;
   if (!request.ParseFromString(packet.data())) {
      logger_->error("[HeadlessContainerListener] failed to parse SignTXMultiRequest");
      SignTXResponse(clientId, packet.id(), reqType, ErrorCode::FailedToParse);
      return false;
   }

   bs::core::wallet::TXMultiSignRequest txMultiReq;
   bs::core::WalletMap walletMap;
   txMultiReq.prevState = request.signerstate();
   for (int i = 0; i < request.walletids_size(); i++) {
      const auto &wallet = walletsMgr_->getWalletById(request.walletids(i));
      if (!wallet) {
         logger_->error("[HeadlessContainerListener] failed to find wallet with id {}", request.walletids(i));
         SignTXResponse(clientId, packet.id(), reqType, ErrorCode::WalletNotFound);
         return false;
      }
      walletMap[wallet->walletId()] = wallet;
   }

   const std::string prompt("Signing multi-wallet input (auth revoke) transaction");

   const auto cbOnAllPasswords = [this, txMultiReq, walletMap, clientId, reqType, id=packet.id()]
                                 (const std::unordered_map<std::string, SecureBinaryData> &walletPasswords) {
      try {
         const auto wallet = walletsMgr_->getWalletById(walletPasswords.begin()->first);
         auto lock = wallet->lockForEncryption(walletPasswords.begin()->second);
         const auto tx = bs::core::SignMultiInputTX(txMultiReq, walletMap);
         SignTXResponse(clientId, id, reqType, ErrorCode::NoError, tx);
      }
      catch (const std::exception &e) {
         logger_->error("[HeadlessContainerListener] failed to sign multi TX request: {}", e.what());
         SignTXResponse(clientId, id, reqType, ErrorCode::InternalError);
      }
   };
   return RequestPasswordsIfNeeded(++reqSeqNo_, clientId, txMultiReq, walletMap, cbOnAllPasswords);
}

bool HeadlessContainerListener::onSignSettlementPayoutTxRequest(const std::string &clientId
   , const headless::RequestPacket &packet)
{
   const auto reqType = headless::SignSettlementPayoutTxType;
   headless::SignSettlementPayoutTxRequest request;

   if (!request.ParseFromString(packet.data())) {
      logger_->error("[{}] failed to parse request", __func__);
      SignTXResponse(clientId, packet.id(), reqType, ErrorCode::FailedToParse);
      return false;
   }

   Internal::PasswordDialogDataWrapper dialogData = request.passworddialogdata();
   dialogData.insert("PayOutType", true);

   bs::core::wallet::TXSignRequest txSignReq;
   txSignReq.walletIds = { walletsMgr_->getPrimaryWallet()->walletId() };

   UTXO utxo;
   utxo.unserialize(request.signpayouttxrequest().input());
   if (utxo.isInitialized()) {
      txSignReq.inputs.push_back(utxo);
   }

   BinaryData serialized = request.signpayouttxrequest().recipient();
   const auto recip = ScriptRecipient::deserialize(serialized);
   txSignReq.recipients.push_back(recip);

   txSignReq.fee = request.signpayouttxrequest().fee();

   if (!txSignReq.isValid()) {
      logger_->error("[HeadlessContainerListener] invalid SignTxRequest");
      SignTXResponse(clientId, packet.id(), reqType, ErrorCode::TxInvalidRequest);
      return false;
   }

   const auto settlData = request.signpayouttxrequest().settlement_data();
   bs::core::wallet::SettlementData sd{ settlData.settlement_id()
      , settlData.counterparty_pubkey(), settlData.my_pubkey_first() };

   const auto onPassword = [this, txSignReq, sd, clientId, id = packet.id(), reqType]
      (bs::error::ErrorCode result, const SecureBinaryData &pass) {
      if (result != ErrorCode::NoError) {
         logger_->error("[HeadlessContainerListener] payout transaction failed, result from ui: {}", static_cast<int>(result));
         SignTXResponse(clientId, id, reqType, result);
         return;
      }

      try {
         const auto wallet = walletsMgr_->getPrimaryWallet();
         if (!wallet->encryptionTypes().empty() && pass.isNull()) {
            logger_->error("[HeadlessContainerListener] empty password for wallet {}", wallet->name());
            SignTXResponse(clientId, id, reqType, ErrorCode::MissingPassword);
            return;
         }
         {
            auto passLock = wallet->lockForEncryption(pass);
            const auto tx = wallet->signSettlementTXRequest(txSignReq, sd);
            SignTXResponse(clientId, id, reqType, ErrorCode::NoError, tx);
         }
      } catch (const std::exception &e) {
         logger_->error("[HeadlessContainerListener] failed to sign payout TX request: {}", e.what());
         SignTXResponse(clientId, id, reqType, ErrorCode::InternalError);
      }
   };

   return RequestPasswordIfNeeded(clientId, txSignReq.walletIds.front(), txSignReq, reqType
      , dialogData, onPassword);
}

bool HeadlessContainerListener::onSignAuthAddrRevokeRequest(const std::string &clientId
   , const headless::RequestPacket &packet)
{
   headless::SignAuthAddrRevokeRequest request;
   if (!request.ParseFromString(packet.data())) {
      logger_->error("[{}] failed to parse request", __func__);
      SignTXResponse(clientId, packet.id(), packet.type(), ErrorCode::FailedToParse);
      return false;
   }

   const auto wallet = walletsMgr_->getWalletById(request.wallet_id());
   if (!wallet) {
      logger_->error("[{}] failed to find auth wallet {}", __func__, request.wallet_id());
      SignTXResponse(clientId, packet.id(), packet.type(), ErrorCode::WalletNotFound);
      return false;
   }

   Internal::PasswordDialogDataWrapper dialogData;
   dialogData.insert("WalletId", request.wallet_id());
   dialogData.insert("AuthAddress", request.auth_address());

   bs::core::wallet::TXSignRequest txSignReq;
   txSignReq.walletIds = { wallet->walletId() };

   UTXO utxo;
   utxo.unserialize(request.utxo());
   if (utxo.isInitialized()) {
      txSignReq.inputs.push_back(utxo);
   }
   else {
      logger_->error("[{}] failed to parse UTXO", __func__);
      SignTXResponse(clientId, packet.id(), packet.type(), ErrorCode::TxInvalidRequest);
      return false;
   }

   const auto onPassword = [this, wallet, utxo, request, clientId, id = packet.id(), reqType = packet.type()]
   (bs::error::ErrorCode result, const SecureBinaryData &pass) {
      if (result != ErrorCode::NoError) {
         logger_->error("[HeadlessContainerListener] auth revoke failed, result from ui: {}", static_cast<int>(result));
         SignTXResponse(clientId, id, reqType, result);
         return;
      }

      ValidationAddressManager validationMgr(nullptr);
      validationMgr.addValidationAddress(request.validation_address());

      try {
         {
            auto passLock = wallet->lockForEncryption(pass);
            const auto tx = AuthAddressLogic::revoke(request.auth_address(), wallet->getResolver()
               , request.validation_address(), utxo);
            SignTXResponse(clientId, id, reqType, ErrorCode::NoError, tx);
         }
      } catch (const std::exception &e) {
         logger_->error("[HeadlessContainerListener] failed to sign payout TX request: {}", e.what());
         SignTXResponse(clientId, id, reqType, ErrorCode::InternalError);
      }
   };

   return RequestPasswordIfNeeded(clientId, txSignReq.walletIds.front(), txSignReq
      , packet.type(), dialogData, onPassword);
}

void HeadlessContainerListener::SignTXResponse(const std::string &clientId, unsigned int id, headless::RequestType reqType
   , bs::error::ErrorCode errorCode, const BinaryData &tx)
{
   headless::SignTxReply response;
   response.set_errorcode(static_cast<uint32_t>(errorCode));

   if (!tx.isNull()) {
      response.set_signedtx(tx.toBinStr());
   }

   headless::RequestPacket packet;
   packet.set_id(id);
   packet.set_type(reqType);
   packet.set_data(response.SerializeAsString());

   if (!sendData(packet.SerializeAsString(), clientId)) {
      logger_->error("[HeadlessContainerListener] failed to send response signTX packet");
   }
   if (errorCode == bs::error::ErrorCode::NoError && callbacks_) {
      callbacks_->txSigned(tx);
   }
}

void HeadlessContainerListener::passwordReceived(const std::string &clientId, const std::string &walletId
   , bs::error::ErrorCode result, const SecureBinaryData &password)
{
   if (deferredPasswordRequests_.empty()) {
      logger_->error("[HeadlessContainerListener::{}] failed to find password received callback {}", __func__);
      return;
   }
   const PasswordReceivedCb &cb = std::move(deferredPasswordRequests_.front().callback);
   if (cb) {
      cb(result, password);
   }

   // at this point password workflow finished for deferredPasswordRequests_.front() dialog
   // now we can remove dialog
   deferredPasswordRequests_.erase(deferredPasswordRequests_.begin());
   deferredDialogRunning_ = false;

   // execute next pw dialog
   RunDeferredPwDialog();
}

void HeadlessContainerListener::passwordReceived(const std::string &walletId
   , bs::error::ErrorCode result, const SecureBinaryData &password)
{
   passwordReceived({}, walletId, result, password);
}

bool HeadlessContainerListener::RequestPasswordIfNeeded(const std::string &clientId, const std::string &walletId
   , const bs::core::wallet::TXSignRequest &txReq
   , headless::RequestType reqType, const Internal::PasswordDialogDataWrapper &dialogData
   , const PasswordReceivedCb &cb)
{
   std::string rootId = walletId;
   const auto wallet = walletsMgr_->getWalletById(walletId);
   bool needPassword = true;
   if (wallet) {
      needPassword = !wallet->encryptionTypes().empty();
      if (needPassword) {
         const auto &hdRoot = walletsMgr_->getHDRootForLeaf(walletId);
         if (hdRoot) {
            rootId = hdRoot->walletId();
         }
      }
   }
   else {
      const auto hdWallet = walletsMgr_->getHDWalletById(walletId);
      if (!hdWallet) {
         logger_->error("[{}] failed to find wallet {}", __func__, walletId);
         return false;
      }
      needPassword = !hdWallet->encryptionTypes().empty();
   }

   auto autoSignCategory = static_cast<bs::signer::AutoSignCategory>(dialogData.value<int>("AutoSignCategory"));
   // currently only dealer can use autosign
   bool autoSignAllowed = (autoSignCategory == bs::signer::AutoSignCategory::SettlementDealer);

   SecureBinaryData password;
   if (autoSignAllowed && needPassword) {
      const auto passwordIt = passwords_.find(rootId);
      if (passwordIt != passwords_.end()) {
         needPassword = false;
         password = passwordIt->second;
      }
   }

   if (!needPassword) {
      if (cb) {
         cb(ErrorCode::NoError, password);
      }
      return true;
   }

   return RequestPassword(rootId, txReq, reqType, dialogData, cb);
}

bool HeadlessContainerListener::RequestPasswordsIfNeeded(int reqId, const std::string &clientId
   , const bs::core::wallet::TXMultiSignRequest &txMultiReq, const bs::core::WalletMap &walletMap
   , const PasswordsReceivedCb &cb)
{
   Internal::PasswordDialogDataWrapper dialogData;

   TempPasswords tempPasswords;
   for (const auto &wallet : walletMap) {
      const auto &walletId = wallet.first;
      const auto &rootWallet = walletsMgr_->getHDRootForLeaf(walletId);
      const auto &rootWalletId = rootWallet->walletId();

      tempPasswords.rootLeaves[rootWalletId].insert(walletId);
      tempPasswords.reqWalletIds.insert(walletId);

      if (!rootWallet->encryptionTypes().empty()) {
         const auto cbWalletPass = [this, reqId, cb, rootWalletId](bs::error::ErrorCode result, const SecureBinaryData &password) {
            auto &tempPasswords = tempPasswords_[reqId];
            const auto &walletsIt = tempPasswords.rootLeaves.find(rootWalletId);
            if (walletsIt == tempPasswords.rootLeaves.end()) {
               return;
            }
            for (const auto &walletId : walletsIt->second) {
               tempPasswords.passwords[walletId] = password;
            }
            if (tempPasswords.passwords.size() == tempPasswords.reqWalletIds.size()) {
               cb(tempPasswords.passwords);
               tempPasswords_.erase(reqId);
            }
         };

         bs::core::wallet::TXSignRequest txReq;
         txReq.walletIds = { rootWallet->walletId() };
         RequestPassword(clientId, txReq, headless::RequestType::SignTxRequestType, dialogData, cbWalletPass);
      }
      else {
         tempPasswords.passwords[walletId] = {};
      }
   }
   if (tempPasswords.reqWalletIds.size() == tempPasswords.passwords.size()) {
      cb(tempPasswords.passwords);
   }
   else {
      tempPasswords_[reqId] = tempPasswords;
   }
   return true;
}

bool HeadlessContainerListener::RequestPassword(const std::string &rootId, const bs::core::wallet::TXSignRequest &txReq
   , headless::RequestType reqType, const Internal::PasswordDialogDataWrapper &dialogData
   , const PasswordReceivedCb &cb)
{
   // TODO:
   // if deferredDialogRunning_ is set to true, no one else pw dialog might be displayed
   // need to implement some timer which will control dialogs queue for case when proxyCallback not fired
   // and deferredDialogRunning_ flag not cleared

   PasswordRequest dialog;

   dialog.dialogData = dialogData;
   dialog.callback = cb;
   dialog.passwordRequest = [this, reqType, dialogData, txReq](){
      if (callbacks_) {
         switch (reqType) {
         case headless::SignTxRequestType:
            callbacks_->decryptWalletRequest(signer::PasswordDialogType::SignTx, dialogData, txReq);
            break;
         case headless::SignPartialTXRequestType:
            callbacks_->decryptWalletRequest(signer::PasswordDialogType::SignPartialTx, dialogData, txReq);
            break;

         case headless::SignSettlementTxRequestType:
         case headless::SignSettlementPayoutTxType:
            callbacks_->decryptWalletRequest(signer::PasswordDialogType::SignSettlementTx, dialogData, txReq);
            break;
         case headless::SignSettlementPartialTxType:
            callbacks_->decryptWalletRequest(signer::PasswordDialogType::SignSettlementPartialTx, dialogData, txReq);
            break;

         case headless::CreateHDLeafRequestType:
            callbacks_->decryptWalletRequest(signer::PasswordDialogType::CreateSettlementLeaf, dialogData);
            break;
         case headless::CreateSettlWalletType:
            callbacks_->decryptWalletRequest(signer::PasswordDialogType::CreateHDLeaf, dialogData);
            break;
         case headless::SetUserIdType:
         case headless::SignAuthAddrRevokeType:
            callbacks_->decryptWalletRequest(signer::PasswordDialogType::CreateAuthLeaf, dialogData, txReq);
            break;
         case headless::PromoteHDWalletRequestType:
            callbacks_->decryptWalletRequest(signer::PasswordDialogType::PromoteHDWallet, dialogData);
            break;

         default:
            logger_->warn("[{}] unknown request for password request: {}", __func__, (int)reqType);
         }
      }
   };

   deferredPasswordRequests_.push_back(dialog);
   RunDeferredPwDialog();

   return true;
}

void HeadlessContainerListener::RunDeferredPwDialog()
{
   if (deferredPasswordRequests_.empty()) {
      return;
   }

   if(!deferredDialogRunning_) {
      deferredDialogRunning_ = true;

      std::sort(deferredPasswordRequests_.begin(), deferredPasswordRequests_.end());
      deferredPasswordRequests_.front().passwordRequest(); // run stored lambda
   }
}

bool HeadlessContainerListener::onSetUserId(const std::string &clientId, headless::RequestPacket &packet)
{
   headless::SetUserIdRequest request;
   if (!request.ParseFromString(packet.data())) {
      logger_->error("[HeadlessContainerListener] failed to parse SetUserIdRequest");
      return false;
   }

   if (request.userid().empty()) {
      logger_->info("[{}] empty user id - do nothing", __func__);
      return true;
   }

   walletsMgr_->setUserId(request.userid());

   const auto wallet = walletsMgr_->getPrimaryWallet();
   if (!wallet) {
      logger_->info("[{}] no primary wallet - aborting", __func__);
      return true;
   }
   const auto group = wallet->getGroup(bs::hd::BlockSettle_Auth);
   if (!group) {
      logger_->error("[{}] primary wallet misses Auth group", __func__);
      setUserIdResponse(clientId, packet.id(), headless::AWR_NoPrimary);
      return false;
   }
   const auto authGroup = std::dynamic_pointer_cast<bs::core::hd::AuthGroup>(group);
   if (!authGroup) {
      logger_->error("[{}] Auth group has wrong type", __func__);
      setUserIdResponse(clientId, packet.id(), headless::AWR_NoPrimary);
      return false;
   }
   const SecureBinaryData salt(request.userid());

   if (salt.isNull()) {
      logger_->debug("[{}] unsetting auth salt", __func__);
      setUserIdResponse(clientId, packet.id(), headless::AWR_UnsetSalt);
      return true;
   }

   logger_->debug("[{}] setting salt {}...", __func__, salt.toHexStr());
   const auto prevSalt = authGroup->getSalt();
   if (prevSalt.isNull()) {
      try {
         authGroup->setSalt(salt);
      } catch (const std::exception &e) {
         logger_->error("[{}] error setting auth salt: {}", __func__, e.what());
         setUserIdResponse(clientId, packet.id(), headless::AWR_SaltSetFailed);
         return false;
      }
   }
   else {
      if (prevSalt == salt) {
         logger_->debug("[{}] salts match - ok", __func__);
      }
      else {
         logger_->error("[{}] salts don't match - aborting for now", __func__);
         setUserIdResponse(clientId, packet.id(), headless::AWR_WrongSalt);
         return false;
      }
   }

   const bs::hd::Path authPath({bs::hd::Purpose::Native, bs::hd::BlockSettle_Auth, 0});
   auto leaf = authGroup->getLeafByPath(authPath);
   if (leaf) {
      const auto authLeaf = std::dynamic_pointer_cast<bs::core::hd::AuthLeaf>(leaf);
      if (authLeaf && (authLeaf->getSalt() == salt)) {
         setUserIdResponse(clientId, packet.id(), headless::AWR_NoError, authLeaf->walletId());
         return true;
      }
      else {
         setUserIdResponse(clientId, packet.id(), headless::AWR_WrongSalt, leaf->walletId());
         return false;
      }
   }
   setUserIdResponse(clientId, packet.id(), headless::AWR_NoPrimary);
   return true;
}

bool HeadlessContainerListener::onSyncCCNames(headless::RequestPacket &packet)
{
   headless::SyncCCNamesData request;
   if (!request.ParseFromString(packet.data())) {
      logger_->error("[{}] failed to parse request", __func__);
      if (callbacks_) {
         callbacks_->ccNamesReceived(false);
      }
      return false;
   }
   logger_->debug("[{}] received {} CCs", __func__, request.ccnames_size());
   std::vector<std::string> ccNames;
   for (int i = 0; i < request.ccnames_size(); ++i) {
      const auto cc = request.ccnames(i);
      ccNames.emplace_back(std::move(cc));
   }
   if (callbacks_) {
      callbacks_->ccNamesReceived(true);
   }
   walletsMgr_->setCCLeaves(ccNames);
   return true;
}

void HeadlessContainerListener::setUserIdResponse(const std::string &clientId, unsigned int id
   , headless::AuthWalletResponseType respType, const std::string &walletId)
{
   headless::SetUserIdResponse response;
   response.set_auth_wallet_id(walletId);
   response.set_response(respType);

   headless::RequestPacket packet;
   packet.set_id(id);
   packet.set_type(headless::SetUserIdType);
   packet.set_data(response.SerializeAsString());
   sendData(packet.SerializeAsString(), clientId);
}

bool HeadlessContainerListener::onCreateHDLeaf(const std::string &clientId
   , Blocksettle::Communication::headless::RequestPacket &packet)
{
   headless::CreateHDLeafRequest request;
   if (!request.ParseFromString(packet.data())) {
      logger_->error("[HeadlessContainerListener] failed to parse CreateHDLeafRequest");
      return false;
   }

   const auto hdWallet = walletsMgr_->getHDWalletById(request.rootwalletid());
   if (!hdWallet) {
      logger_->error("[HeadlessContainerListener] failed to find root HD wallet by id {}", request.rootwalletid());
      CreateHDLeafResponse(clientId, packet.id(), ErrorCode::WalletNotFound);
      return false;
   }
   const auto path = bs::hd::Path::fromString(request.path());
   if ((path.length() < 3) && !path.isAbsolute()) {
      logger_->error("[HeadlessContainerListener] invalid path {} at HD wallet creation", request.path());
      CreateHDLeafResponse(clientId, packet.id(), ErrorCode::InternalError);
      return false;
   }

   const auto onPassword = [this, hdWallet, path, clientId, id = packet.id(), salt=request.salt()]
      (bs::error::ErrorCode result, const SecureBinaryData &pass)
   {
      std::shared_ptr<bs::core::hd::Node> leafNode;
      if (result != ErrorCode::NoError) {
         logger_->error("[HeadlessContainerListener] no password for encrypted wallet");
         CreateHDLeafResponse(clientId, id, result);
         return;
      }

      const auto groupIndex = static_cast<bs::hd::CoinType>(path.get(1));
      auto group = hdWallet->getGroup(groupIndex);
      if (!group) {
         group = hdWallet->createGroup(groupIndex);
      }

      try {
         if (!salt.empty()) {
            const auto authGroup = std::dynamic_pointer_cast<bs::core::hd::AuthGroup>(group);
            if (authGroup) {
               const auto prevSalt = authGroup->getSalt();
               if (prevSalt.isNull()) {
                  authGroup->setSalt(salt);
               }
               else if (prevSalt != salt) {
                  logger_->error("[HeadlessContainerListener] auth salts mismatch");
                  CreateHDLeafResponse(clientId, id, ErrorCode::MissingAuthKeys);
                  return;
               }
            }
         }

         auto leaf = group->getLeafByPath(path);

         if (leaf == nullptr) {
            auto lock = hdWallet->lockForEncryption(pass);
            leaf = group->createLeaf(path);

            if (leaf == nullptr) {
               logger_->error("[HeadlessContainerListener] failed to create/get leaf {}", path.toString());
               CreateHDLeafResponse(clientId, id, ErrorCode::InternalError);
               return;
            }

            if (callbacks_) {
               callbacks_->walletChanged(leaf->walletId());
            }

            if ((path.get(1) | bs::hd::hardFlag) == bs::hd::CoinType::BlockSettle_Auth) {
               createSettlementLeaves(hdWallet, leaf->getPooledAddressList());
            }
         }

         auto assetPtr = leaf->getRootAsset();

         auto rootPtr = std::dynamic_pointer_cast<AssetEntry_BIP32Root>(assetPtr);
         if (rootPtr == nullptr) {
            throw AssetException("unexpected root asset type");
         }

         CreateHDLeafResponse(clientId, id, ErrorCode::NoError, leaf);
      }
      catch (const std::exception &e) {
         logger_->error("[HeadlessContainerListener::CreateHDLeaf] failed: {}", e.what());
         CreateHDLeafResponse(clientId, id, ErrorCode::WalletNotFound);
      }
   };

   RequestPasswordIfNeeded(clientId, request.rootwalletid(), {}, headless::CreateHDLeafRequestType, request.passworddialogdata(), onPassword);
   return true;
}

bool HeadlessContainerListener::createSettlementLeaves(const std::shared_ptr<bs::core::hd::Wallet> &wallet
   , const std::vector<bs::Address> &authAddresses)
{
   if (!wallet) {
      return false;
   }
   logger_->debug("[{}] creating settlement leaves for {} auth address[es]", __func__
      , authAddresses.size());
   for (const auto &authAddr : authAddresses) {
      try {
         const auto leaf = wallet->createSettlementLeaf(authAddr);
         if (!leaf) {
            logger_->error("[{}] failed to create settlement leaf for {}"
               , __func__, authAddr.display());
            return false;
         }
         if (callbacks_) {
            callbacks_->walletChanged(leaf->walletId());
         }
      }
      catch (const std::exception &e) {
         logger_->error("[{}] failed to create settlement leaf for {}: {}", __func__
            , authAddr.display(), e.what());
         return false;
      }
   }
   return true;
}

bool HeadlessContainerListener::createAuthLeaf(const std::shared_ptr<bs::core::hd::Wallet> &wallet
   , const BinaryData &salt, const SecureBinaryData &password)
{
   if (salt.isNull()) {
      logger_->error("[{}] can't create auth leaf with empty salt", __func__);
      return false;
   }
   const auto group = wallet->getGroup(bs::hd::BlockSettle_Auth);
   if (!group) {
      logger_->error("[{}] primary wallet misses Auth group", __func__);
      return false;
   }
   const auto authGroup = std::dynamic_pointer_cast<bs::core::hd::AuthGroup>(group);
   if (!authGroup) {
      logger_->error("[{}] Auth group has wrong type", __func__);
      return false;
   }

   const auto prevSalt = authGroup->getSalt();
   if (prevSalt.isNull()) {
      try {
         authGroup->setSalt(salt);
      } catch (const std::exception &e) {
         logger_->error("[{}] error setting auth salt: {}", __func__, e.what());
         return false;
      }
   } else {
      if (prevSalt != salt) {
         logger_->error("[{}] salts don't match - aborting", __func__);
         return false;
      }
   }

   const bs::hd::Path authPath({ bs::hd::Purpose::Native, bs::hd::BlockSettle_Auth, 0 });
   auto leaf = authGroup->getLeafByPath(authPath);
   if (leaf) {
      const auto authLeaf = std::dynamic_pointer_cast<bs::core::hd::AuthLeaf>(leaf);
      if (authLeaf && (authLeaf->getSalt() == salt)) {
         logger_->debug("[{}] auth leaf for {} aready exists", __func__, salt.toHexStr());
         return true;
      } else {
         logger_->error("[{}] auth leaf salts mismatch", __func__);
         return false;
      }
   }

   try {
      auto lock = wallet->lockForEncryption(password);
      auto leaf = group->createLeaf(AddressEntryType_Default, 0 + bs::hd::hardFlag, 5);
      if (leaf) {
         return createSettlementLeaves(wallet, leaf->getPooledAddressList());
      } else {
         logger_->error("[HeadlessContainerListener::onSetUserId] failed to create auth leaf");
      }
   } catch (const std::exception &e) {
      logger_->error("[HeadlessContainerListener::onSetUserId] failed to create auth leaf: {}", e.what());
   }
   return false;
}

bool HeadlessContainerListener::onPromoteHDWallet(const std::string& clientId, headless::RequestPacket& packet)
{
   headless::PromoteHDWalletRequest request;
   if (!request.ParseFromString(packet.data())) {
      logger_->error("[HeadlessContainerListener] failed to parse PromoteHDWalletRequest");
      return false;
   }

   const std::string &walletId = request.rootwalletid();
   const auto hdWallet = walletsMgr_->getHDWalletById(walletId);
   if (!hdWallet) {
      logger_->error("[HeadlessContainerListener] failed to find root HD wallet by id {}", walletId);
      CreatePromoteHDWalletResponse(clientId, packet.id(), ErrorCode::WalletNotFound, walletId);
      return false;
   }

   const auto onPassword = [this, hdWallet, walletId, clientId, id = packet.id(), userId=request.user_id()]
      (bs::error::ErrorCode result, const SecureBinaryData &pass)
   {
      std::shared_ptr<bs::core::hd::Node> leafNode;
      if (result != ErrorCode::NoError) {
         logger_->error("[HeadlessContainerListener] no password for encrypted wallet");
         CreatePromoteHDWalletResponse(clientId, id, result, walletId);
         return;
      }

      auto group = hdWallet->getGroup(bs::hd::BlockSettle_Auth);
      if (!group) {
         group = hdWallet->createGroup(bs::hd::BlockSettle_Auth);
      }
      if (!createAuthLeaf(hdWallet, userId, pass)) {
         logger_->error("[HeadlessContainerListener::onPromoteHDWallet] failed to create auth leaf");
      }

      if (!walletsMgr_->ccLeaves().empty()) {
         logger_->debug("[HeadlessContainerListener::onPromoteHDWallet] creating {} CC leaves"
            , walletsMgr_->ccLeaves().size());
         group = hdWallet->createGroup(bs::hd::BlockSettle_CC);
         if (group) {
            auto lock = hdWallet->lockForEncryption(pass);
            for (const auto &cc : walletsMgr_->ccLeaves()) {
               try {
                  group->createLeaf(AddressEntryType_P2WPKH, cc);
               }  // existing leaf creation failure is ignored
               catch (...) {}
            }
         }
         else {
            logger_->error("[HeadlessContainerListener::onPromoteHDWallet] failed to create CC group");
         }
      }
      CreatePromoteHDWalletResponse(clientId, id, ErrorCode::NoError, walletId);
      walletsListUpdated();
   };

   RequestPasswordIfNeeded(clientId, request.rootwalletid(), {}, headless::PromoteHDWalletRequestType
      , request.passworddialogdata(), onPassword);
   return true;
}

void HeadlessContainerListener::CreateHDLeafResponse(const std::string &clientId, unsigned int id
   , ErrorCode result, const std::shared_ptr<bs::core::hd::Leaf>& leaf)
{
   headless::CreateHDLeafResponse response;
   if (result != bs::error::ErrorCode::NoError && leaf) {
      const std::string pathString = leaf->path().toString();
      logger_->debug("[HeadlessContainerListener] CreateHDWalletResponse: {}", pathString);

      auto leafResponse = response.mutable_leaf();

      leafResponse->set_path(pathString);
      leafResponse->set_walletid(leaf->walletId());
   }
   response.set_errorcode(static_cast<uint32_t>(result));

   headless::RequestPacket packet;
   packet.set_id(id);
   packet.set_type(headless::CreateHDLeafRequestType);
   packet.set_data(response.SerializeAsString());

   if (!sendData(packet.SerializeAsString(), clientId)) {
      logger_->error("[HeadlessContainerListener] failed to send response CreateHDLeaf packet");
   }
}

void HeadlessContainerListener::CreatePromoteHDWalletResponse(const std::string& clientId, unsigned int id
   , ErrorCode result, const std::string& walletId)
{
   logger_->debug("[HeadlessContainerListener] PromoteHDWalletResponse: {}", id);
   headless::PromoteHDWalletResponse response;
   response.set_rootwalletid(walletId);
   response.set_errorcode(static_cast<uint32_t>(result));

   headless::RequestPacket packet;
   packet.set_id(id);
   packet.set_type(headless::PromoteHDWalletRequestType);
   packet.set_data(response.SerializeAsString());

   if (!sendData(packet.SerializeAsString(), clientId)) {
      logger_->error("[HeadlessContainerListener] failed to send response PromoteHDWallet packet");
   }
}

static SecureBinaryData getPubKey(const std::shared_ptr<bs::core::hd::Leaf> &leaf)
{
   auto rootPtr = leaf->getRootAsset();
   auto rootSingle = std::dynamic_pointer_cast<AssetEntry_Single>(rootPtr);
   if (rootSingle == nullptr) {
      return {};
   }
   return rootSingle->getPubKey()->getCompressedKey();
}

bool HeadlessContainerListener::onCreateSettlWallet(const std::string &clientId, headless::RequestPacket packet)
{
   headless::CreateSettlWalletRequest request;
   if (!request.ParseFromString(packet.data())) {
      logger_->error("[{}] failed to parse request", __func__);
      return false;
   }
   const auto priWallet = walletsMgr_->getPrimaryWallet();
   if (!priWallet) {
      logger_->error("[{}] no primary wallet found", __func__);
      packet.set_data("");
      sendData(packet.SerializeAsString(), clientId);
      return false;
   }
   const auto settlLeaf = priWallet->getSettlementLeaf(request.auth_address());
   if (settlLeaf) {
      headless::CreateSettlWalletResponse response;
      response.set_wallet_id(settlLeaf->walletId());
      response.set_public_key(getPubKey(settlLeaf).toBinStr());
      packet.set_data(response.SerializeAsString());
      sendData(packet.SerializeAsString(), clientId);
      return true;
   }

   auto &reqsForAddress = settlLeafReqs_[{clientId, request.auth_address() }];
   reqsForAddress.push_back(packet.id());
   if (reqsForAddress.size() > 1) {
      return true;
   }
   const auto &onPassword = [this, priWallet, clientId, request, id=packet.id()]
      (bs::error::ErrorCode result, const SecureBinaryData &password) {
      headless::CreateSettlWalletResponse response;
      headless::RequestPacket packet;
      packet.set_id(id);
      packet.set_type(headless::CreateSettlWalletType);

      const auto &itReqs = settlLeafReqs_.find({ clientId, request.auth_address() });
      if (itReqs == settlLeafReqs_.end()) {
         logger_->warn("[HeadlessContainerListener] failed to find list of requests");
         packet.set_data(response.SerializeAsString());
         sendData(packet.SerializeAsString(), clientId);
         return;
      }

      const auto &sendAllIds = [this, clientId](const std::string &response
         , const std::vector<uint32_t> &ids)
      {
         if (ids.empty()) {
            return;
         }
         headless::RequestPacket packet;
         packet.set_data(response);
         packet.set_type(headless::CreateSettlWalletType);
         for (const auto &id : ids) {
            packet.set_id(id);
            sendData(packet.SerializeAsString(), clientId);
         }
      };

      if (result != bs::error::ErrorCode::NoError) {
         logger_->warn("[HeadlessContainerListener] password request failed");
         sendAllIds(response.SerializeAsString(), itReqs->second);
         return;
      }

      {
         auto lock = priWallet->lockForEncryption(password);
         const auto leaf = priWallet->createSettlementLeaf(request.auth_address());
         if (!leaf) {
            logger_->error("[HeadlessContainerListener] failed to create settlement leaf for {}"
               , request.auth_address());
            sendAllIds(response.SerializeAsString(), itReqs->second);
            return;
         }
         response.set_wallet_id(leaf->walletId());
         response.set_public_key(getPubKey(leaf).toBinStr());
      }
      if (callbacks_) {
         callbacks_->walletChanged(response.wallet_id());
      }
      sendAllIds(response.SerializeAsString(), itReqs->second);
      settlLeafReqs_.erase(itReqs);
   };

   Internal::PasswordDialogDataWrapper dialogData = request.passworddialogdata();
   dialogData.insert("WalletId", priWallet->walletId());
   dialogData.insert("AuthAddress", request.auth_address());

   return RequestPasswordIfNeeded(clientId, priWallet->walletId(), {}, headless::CreateSettlWalletType
      , dialogData, onPassword);
}

bool HeadlessContainerListener::onSetSettlementId(const std::string &clientId
   , Blocksettle::Communication::headless::RequestPacket packet)
{
   headless::SetSettlementIdRequest request;
   if (!request.ParseFromString(packet.data())) {
      logger_->error("[{}] failed to parse request", __func__);
      return false;
   }
   headless::SetSettlementIdResponse response;
   response.set_success(false);

   const auto leaf = walletsMgr_->getWalletById(request.wallet_id());
   const auto settlLeaf = std::dynamic_pointer_cast<bs::core::hd::SettlementLeaf>(leaf);
   if (settlLeaf == nullptr) {
      logger_->error("[{}] no leaf for id {}", __func__, request.wallet_id());
      packet.set_data(response.SerializeAsString());
      sendData(packet.SerializeAsString(), clientId);
      return false;
   }

   // Call addSettlementID only once, otherwise addSettlementID will crash
   if (settlLeaf->getIndexForSettlementID(request.settlement_id()) == UINT32_MAX) {
      settlLeaf->addSettlementID(request.settlement_id());
      settlLeaf->getNewExtAddress();
      if (callbacks_) {
         callbacks_->walletChanged(settlLeaf->walletId());
      }
   }

   response.set_success(true);
   response.set_wallet_id(leaf->walletId());

   packet.set_data(response.SerializeAsString());
   return sendData(packet.SerializeAsString(), clientId);
}

bool HeadlessContainerListener::onGetPayinAddr(const std::string &clientId
   , Blocksettle::Communication::headless::RequestPacket packet)
{
   headless::SettlPayinAddressRequest request;
   if (!request.ParseFromString(packet.data())) {
      logger_->error("[{}] failed to parse request", __func__);
      return false;
   }
   headless::SettlPayinAddressResponse response;
   response.set_success(false);

   const auto wallet = walletsMgr_->getHDWalletById(request.wallet_id());
   if (!wallet) {
      logger_->error("[{}] no hd wallet for id {}", __func__, request.wallet_id());
      packet.set_data(response.SerializeAsString());
      sendData(packet.SerializeAsString(), clientId);
      return false;
   }
   const bs::core::wallet::SettlementData sd { request.settlement_data().settlement_id()
      , request.settlement_data().counterparty_pubkey()
      , request.settlement_data().my_pubkey_first() };
   const auto addr = wallet->getSettlementPayinAddress(sd);
   response.set_address(addr.display());
   response.set_wallet_id(wallet->walletId());
   response.set_success(true);

   packet.set_data(response.SerializeAsString());
   return sendData(packet.SerializeAsString(), clientId);
}

bool HeadlessContainerListener::onSettlGetRootPubkey(const std::string &clientId
   , Blocksettle::Communication::headless::RequestPacket packet)
{
   headless::SettlGetRootPubkeyRequest request;
   if (!request.ParseFromString(packet.data())) {
      logger_->error("[{}] failed to parse request", __func__);
      return false;
   }
   headless::SettlGetRootPubkeyResponse response;
   response.set_success(false);
   if (!request.wallet_id().empty()) {
      const auto leaf = walletsMgr_->getWalletById(request.wallet_id());
      if (!leaf) {
         logger_->error("[{}] no leaf for id {}", __func__, request.wallet_id());
         packet.set_data(response.SerializeAsString());
         sendData(packet.SerializeAsString(), clientId);
         return false;
      }
      response.set_success(true);
      response.set_wallet_id(leaf->walletId());
      response.set_public_key(getPubKey(leaf).toBinStr());
   }
   packet.set_data(response.SerializeAsString());
   return sendData(packet.SerializeAsString(), clientId);
}

// FIXME: needs to review and reimplement setLimits at all
//bool HeadlessContainerListener::onSetLimits(const std::string &clientId, headless::RequestPacket &packet)
//{
//   headless::SetLimitsRequest request;
//   if (!request.ParseFromString(packet.data())) {
//      logger_->error("[HeadlessContainerListener] failed to parse SetLimitsRequest");
//      AutoSignActiveResponse(clientId, {}, false, "request parse error", packet.id());
//      return false;
//   }
//   if (request.rootwalletid().empty()) {
//      logger_->error("[HeadlessContainerListener] no wallet specified in SetLimitsRequest");
//      AutoSignActiveResponse(clientId, request.rootwalletid(), false, "invalid request", packet.id());
//      return false;
//   }
//   if (!request.activateautosign()) {
//      deactivateAutoSign(clientId, request.rootwalletid());
//      return true;
//   }

//   if (!request.password().empty()) {
//      activateAutoSign(clientId, request.rootwalletid(), BinaryData::CreateFromHex(request.password()));
//   }
//   else {
//      const auto &wallet = walletsMgr_->getHDWalletById(request.rootwalletid());
//      if (!wallet) {
//         logger_->error("[HeadlessContainerListener] failed to find root wallet by id {} (to activate auto-sign)"
//            , request.rootwalletid());
//         AutoSignActiveResponse(clientId, request.rootwalletid(), false, "missing wallet", packet.id());
//         return false;
//      }
//      if (!wallet->encryptionTypes().empty() && !isAutoSignActive(request.rootwalletid())) {
//         addPendingAutoSignReq(request.rootwalletid());
//         if (callbacks_) {
//            bs::core::wallet::TXSignRequest txReq;
//            txReq.walletId = request.rootwalletid();
//            txReq.autoSign = true;
//            callbacks_->pwd(txReq, {});
//         }
//      }
//      else {
//         if (callbacks_) {
//            //callbacks_->asAct(request.rootwalletid());
//         }
//         AutoSignActiveResponse(clientId, request.rootwalletid(), true, {}, packet.id());
//      }
//   }
//   return true;
//}

bool HeadlessContainerListener::onGetHDWalletInfo(const std::string &clientId, headless::RequestPacket &packet)
{
   headless::GetHDWalletInfoRequest request;
   if (!request.ParseFromString(packet.data())) {
      logger_->error("[HeadlessContainerListener] failed to parse GetHDWalletInfoRequest");
      GetHDWalletInfoResponse(clientId, packet.id(), {}, nullptr, "failed to parse request");
      return false;
   }
   const auto &wallet = walletsMgr_->getHDWalletById(request.rootwalletid());
   if (!wallet) {
      logger_->error("[HeadlessContainerListener] failed to find wallet for id {}", request.rootwalletid());
      GetHDWalletInfoResponse(clientId, packet.id(), request.rootwalletid(), nullptr, "failed to find wallet");
      return false;
   }
   GetHDWalletInfoResponse(clientId, packet.id(), request.rootwalletid(), wallet);
   return true;
}

void HeadlessContainerListener::GetHDWalletInfoResponse(const std::string &clientId, unsigned int id
   , const std::string &walletId, const std::shared_ptr<bs::core::hd::Wallet> &wallet, const std::string &error)
{
   headless::GetHDWalletInfoResponse response;
   if (!error.empty()) {
      response.set_error(error);
   }
   if (wallet) {
      for (const auto &encType : wallet->encryptionTypes()) {
         response.add_enctypes(static_cast<uint32_t>(encType));
      }
      for (const auto &encKey : wallet->encryptionKeys()) {
         response.add_enckeys(encKey.toBinStr());
      }
      response.set_rankm(wallet->encryptionRank().first);
      response.set_rankn(wallet->encryptionRank().second);
   }
   if (!walletId.empty()) {
      response.set_rootwalletid(walletId);
   }

   headless::RequestPacket packet;
   packet.set_id(id);
   packet.set_type(headless::GetHDWalletInfoRequestType);
   packet.set_data(response.SerializeAsString());

   if (!sendData(packet.SerializeAsString(), clientId)) {
      logger_->error("[HeadlessContainerListener::{}] failed to send to {}", __func__
         , BinaryData(clientId).toHexStr());
   }
}

void HeadlessContainerListener::AutoSignActivatedEvent(const std::string &walletId, bool active)
{
   headless::AutoSignActEvent autoSignActEvent;
   autoSignActEvent.set_rootwalletid(walletId);
   autoSignActEvent.set_autosignactive(active);

   headless::RequestPacket packet;
   packet.set_type(headless::AutoSignActType);
   packet.set_data(autoSignActEvent.SerializeAsString());

   sendData(packet.SerializeAsString());
}

bool HeadlessContainerListener::CheckSpendLimit(uint64_t value, const std::string &walletId)
{  
   if (isAutoSignActive(walletId)) {
      if (value > limits_.autoSignSpendXBT) {
         logger_->warn("[HeadlessContainerListener] requested auto-sign spend {} exceeds limit {}", value
            , limits_.autoSignSpendXBT);
         deactivateAutoSign(walletId, ErrorCode::TxSpendLimitExceed);
         return false;
      }
   }
   else {
      if (value > limits_.manualSpendXBT) {
         logger_->warn("[HeadlessContainerListener] requested manual spend {} exceeds limit {}", value
            , limits_.manualSpendXBT);
         return false;
      }
   }
   return true;
}

void HeadlessContainerListener::onXbtSpent(int64_t value, bool autoSign)
{
   if (autoSign) {
      limits_.autoSignSpendXBT -= value;
      logger_->debug("[HeadlessContainerListener] new auto-sign spend limit =  {}", limits_.autoSignSpendXBT);
   }
   else {
      limits_.manualSpendXBT -= value;
      logger_->debug("[HeadlessContainerListener] new manual spend limit =  {}", limits_.manualSpendXBT);
   }
}

bs::error::ErrorCode HeadlessContainerListener::activateAutoSign(const std::string &walletId
   , const SecureBinaryData &password)
{
   logger_->info("Activate AutoSign for {}", walletId);

   const auto &wallet = walletId.empty() ? walletsMgr_->getPrimaryWallet() : walletsMgr_->getHDWalletById(walletId);
   if (!wallet) {
      return ErrorCode::WalletNotFound;
   }
   passwords_[wallet->walletId()] = password;

   // multicast event
   AutoSignActivatedEvent(walletId, true);

   return ErrorCode::NoError;
}

bs::error::ErrorCode HeadlessContainerListener::deactivateAutoSign(const std::string &walletId
   , bs::error::ErrorCode reason)
{
   logger_->info("Deactivate AutoSign for {} (error code: {})", walletId, static_cast<int>(reason));

   if (walletId.empty()) {
      passwords_.clear();
   }
   else {
      passwords_.erase(walletId);
   }

   // multicast event
   AutoSignActivatedEvent(walletId, false);

   return ErrorCode::NoError;
}

bool HeadlessContainerListener::isAutoSignActive(const std::string &walletId) const
{
   if (walletId.empty()) {
      return !passwords_.empty();
   }
   return (passwords_.find(walletId) != passwords_.end());
}

void HeadlessContainerListener::walletsListUpdated()
{
   logger_->debug("send WalletsListUpdatedType message");

   headless::RequestPacket packet;
   packet.set_type(headless::WalletsListUpdatedType);
   sendData(packet.SerializeAsString());
}

void HeadlessContainerListener::resetConnection(ServerConnection *connection)
{
   connection_ = connection;
}

static headless::NetworkType mapFrom(NetworkType netType)
{
   switch (netType) {
   case NetworkType::MainNet: return headless::MainNetType;
   case NetworkType::TestNet:
   default:    return headless::TestNetType;
   }
}

bool HeadlessContainerListener::onSyncWalletInfo(const std::string &clientId, headless::RequestPacket packet)
{
   headless::SyncWalletInfoResponse response;

   for (size_t i = 0; i < walletsMgr_->getHDWalletsCount(); ++i) {
      const auto hdWallet = walletsMgr_->getHDWallet(i);
      auto walletData = response.add_wallets();
      walletData->set_format(headless::WalletFormatHD);
      walletData->set_id(hdWallet->walletId());
      walletData->set_name(hdWallet->name());
      walletData->set_description(hdWallet->description());
      walletData->set_nettype(mapFrom(hdWallet->networkType()));
      walletData->set_watching_only(hdWallet->isWatchingOnly());
   }

   packet.set_data(response.SerializeAsString());
   return sendData(packet.SerializeAsString(), clientId);
}

bool HeadlessContainerListener::onSyncHDWallet(const std::string &clientId, headless::RequestPacket packet)
{
   headless::SyncWalletRequest request;
   if (!request.ParseFromString(packet.data())) {
      logger_->error("[{}] failed to parse request", __func__);
      return false;
   }

   headless::SyncHDWalletResponse response;
   const auto hdWallet = walletsMgr_->getHDWalletById(request.walletid());
   if (hdWallet) {
      response.set_walletid(hdWallet->walletId());
      for (const auto &group : hdWallet->getGroups()) {
         auto groupData = response.add_groups();
         groupData->set_type(group->index() | bs::hd::hardFlag);
         groupData->set_ext_only(hdWallet->isExtOnly());

         if (group->index() == bs::hd::CoinType::BlockSettle_Auth) {
            const auto authGroup = std::dynamic_pointer_cast<bs::core::hd::AuthGroup>(group);
            if (authGroup) {
               groupData->set_salt(authGroup->getSalt().toBinStr());
            }
         }

         if (static_cast<bs::hd::CoinType>(group->index()) == bs::hd::CoinType::BlockSettle_Auth) {
            continue;      // don't sync leaves for auth before setUserId is asked
         }
         for (const auto &leaf : group->getLeaves()) {
            auto leafData = groupData->add_leaves();
            leafData->set_id(leaf->walletId());
            leafData->set_path(leaf->path().toString());

            if (groupData->type() == bs::hd::CoinType::BlockSettle_Settlement) {
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
               leafData->set_extra_data(authAddr.toBinStr());
            }
         }
      }
   } else {
      logger_->error("[{}] failed to find HD wallet with id {}", __func__, request.walletid());
      return false;
   }

   packet.set_data(response.SerializeAsString());
   return sendData(packet.SerializeAsString(), clientId);
}

static headless::EncryptionType mapFrom(bs::wallet::EncryptionType encType)
{
   switch (encType) {
   case bs::wallet::EncryptionType::Password:   return headless::EncryptionTypePassword;
   case bs::wallet::EncryptionType::Auth:       return headless::EncryptionTypeAutheID;
   case bs::wallet::EncryptionType::Unencrypted:
   default:       return headless::EncryptionTypeUnencrypted;
   }
}

bool HeadlessContainerListener::onSyncWallet(const std::string &clientId, headless::RequestPacket packet)
{
   headless::SyncWalletRequest request;
   if (!request.ParseFromString(packet.data())) {
      logger_->error("[{}] failed to parse request", __func__);
      return false;
   }

   const auto wallet = walletsMgr_->getWalletById(request.walletid());
   if (!wallet) {
      logger_->error("[{}] failed to find wallet with id {}", __func__, request.walletid());
      return false;
   }

   const auto &lbdSend = [this, wallet, id=packet.id(), clientId]
   {
      headless::SyncWalletResponse response;

      response.set_walletid(wallet->walletId());
      for (const auto &encType : wallet->encryptionTypes()) {
         response.add_encryptiontypes(mapFrom(encType));
      }
      for (const auto &encKey : wallet->encryptionKeys()) {
         response.add_encryptionkeys(encKey.toBinStr());
      }
      auto keyrank = response.mutable_keyrank();
      keyrank->set_m(wallet->encryptionRank().first);
      keyrank->set_n(wallet->encryptionRank().second);

      response.set_nettype(mapFrom(wallet->networkType()));
      response.set_highest_ext_index(wallet->getExtAddressCount());
      response.set_highest_int_index(wallet->getIntAddressCount());

      for (const auto &addr : wallet->getUsedAddressList()) {
         const auto comment = wallet->getAddressComment(addr);
         const auto index = wallet->getAddressIndex(addr);
         auto addrData = response.add_addresses();
         addrData->set_address(addr.display());
         addrData->set_index(index);
         if (!comment.empty()) {
            addrData->set_comment(comment);
         }
      }
      const auto &pooledAddresses = wallet->getPooledAddressList();
      for (const auto &addr : pooledAddresses) {
         const auto index = wallet->getAddressIndex(addr);
         auto addrData = response.add_addrpool();
         addrData->set_address(addr.display());
         addrData->set_index(index);
      }
      for (const auto &txComment : wallet->getAllTxComments()) {
         auto txCommData = response.add_txcomments();
         txCommData->set_txhash(txComment.first.toBinStr());
         txCommData->set_comment(txComment.second);
      }

      headless::RequestPacket packet;
      packet.set_id(id);
      packet.set_data(response.SerializeAsString());
      packet.set_type(headless::SyncWalletType);
      sendData(packet.SerializeAsString(), clientId);
   };
   lbdSend();
   return true;
}

bool HeadlessContainerListener::onSyncComment(const std::string &clientId, headless::RequestPacket packet)
{
   headless::SyncCommentRequest request;
   if (!request.ParseFromString(packet.data())) {
      logger_->error("[{}] failed to parse request", __func__);
      return false;
   }
   const auto wallet = walletsMgr_->getWalletById(request.walletid());
   if (!wallet) {
      logger_->error("[{}] failed to find wallet with id {}", __func__, request.walletid());
      return false;
   }
   bool rc = false;
   if (!request.address().empty()) {
      rc = wallet->setAddressComment(request.address(), request.comment());
      logger_->debug("[{}] comment for address {} is set: {}", __func__, request.address(), rc);
   }
   else {
      rc = wallet->setTransactionComment(request.txhash(), request.comment());
      logger_->debug("[{}] comment for TX {} is set: {}", __func__, BinaryData(request.txhash()).toHexStr(true), rc);
   }
   return rc;
}

void HeadlessContainerListener::SyncAddrsResponse(const std::string &clientId
   , unsigned int id, const std::string &walletId, bs::sync::SyncState state)
{
   headless::SyncAddressesResponse response;
   response.set_wallet_id(walletId);
   headless::SyncState respState = headless::SyncState_Failure;
   switch (state) {
   case bs::sync::SyncState::Success:
      respState = headless::SyncState_Success;
      break;
   case bs::sync::SyncState::NothingToDo:
      respState = headless::SyncState_NothingToDo;
      break;
   case bs::sync::SyncState::Failure:
      respState = headless::SyncState_Failure;
      break;
   }
   response.set_state(respState);

   headless::RequestPacket packet;
   packet.set_id(id);
   packet.set_data(response.SerializeAsString());
   packet.set_type(headless::SyncAddressesType);
   sendData(packet.SerializeAsString(), clientId);
}

bool HeadlessContainerListener::onSyncAddresses(const std::string &clientId, headless::RequestPacket packet)
{
   headless::SyncAddressesRequest request;
   if (!request.ParseFromString(packet.data())) {
      logger_->error("[{}] failed to parse request", __func__);
      return false;
   }
   const auto wallet = walletsMgr_->getWalletById(request.wallet_id());
   if (wallet == nullptr) {
      SyncAddrsResponse(clientId, packet.id(), request.wallet_id(), bs::sync::SyncState::Failure);
      logger_->error("[{}] wallet with ID {} not found", __func__, request.wallet_id());
      return false;
   }

   std::set<BinaryData> addrSet;
   for (int i = 0; i < request.addresses_size(); ++i) {
      addrSet.insert(request.addresses(i));
   }

   //resolve the path and address type for addrSet
   std::map<BinaryData, bs::hd::Path> parsedMap;
   try {
      parsedMap = std::move(wallet->indexPath(addrSet));
   } catch (AccountException &e) {
      //failure to find even on of the addresses means the wallet chain needs 
      //extended further
      SyncAddrsResponse(clientId, packet.id(), request.wallet_id(), bs::sync::SyncState::Failure);
      logger_->error("[{}] failed to find indices for {} addresses in {}: {}"
         , __func__, addrSet.size(), request.wallet_id(), e.what());
      return false;
   }

   std::map<bs::hd::Path::Elem, std::set<bs::hd::Path>> mapByPath;
   for (auto& parsedPair : parsedMap) {
      auto elem = parsedPair.second.get(-2);
      auto& mapping = mapByPath[elem];
      mapping.insert(parsedPair.second);
   }

   //request each chain for the relevant address types
   bool update = false;
   for (auto& mapping : mapByPath) {
      for (auto& path : mapping.second) {
         auto resultPair = wallet->synchronizeUsedAddressChain(path.toString());
         update |= resultPair.second;
      }
   }

   if (update) {
      if (callbacks_) {
         callbacks_->walletChanged(wallet->walletId());
      }
      SyncAddrsResponse(clientId, packet.id(), request.wallet_id(), bs::sync::SyncState::Success);
   }
   else {
      SyncAddrsResponse(clientId, packet.id(), request.wallet_id(), bs::sync::SyncState::NothingToDo);
   }
   return true;
}

bool HeadlessContainerListener::onExtAddrChain(const std::string &clientId, headless::RequestPacket packet)
{
   headless::ExtendAddressChainRequest request;
   if (!request.ParseFromString(packet.data())) {
      logger_->error("[{}] failed to parse request", __func__);
      return false;
   }
   const auto wallet = walletsMgr_->getWalletById(request.wallet_id());
   if (wallet == nullptr) {
      logger_->error("[{}] wallet with ID {} not found", __func__, request.wallet_id());
      return false;
   }

   const auto &lbdSend = [this, wallet, request, id=packet.id(), clientId] {
      headless::ExtendAddressChainResponse response;
      response.set_wallet_id(wallet->walletId());

      try {
         auto&& newAddrVec = wallet->extendAddressChain(request.count(), request.ext_int());

         if (callbacks_) {
            callbacks_->walletChanged(wallet->walletId());
         }

         for (const auto &addr : newAddrVec) {
            auto &&index = wallet->getAddressIndex(addr);
            auto addrData = response.add_addresses();
            addrData->set_address(addr.display());
            addrData->set_index(index);
         }
      }
      catch (const std::exception &e) {
         logger_->error("[HeadlessContainerListener::onExtAddrChain] failed: {}", e.what());
      }

      headless::RequestPacket packet;
      packet.set_id(id);
      packet.set_type(headless::ExtendAddressChainType);
      packet.set_data(response.SerializeAsString());
      sendData(packet.SerializeAsString(), clientId);
   };
   lbdSend();
   return true;
}

bool HeadlessContainerListener::onSyncNewAddr(const std::string &clientId, headless::RequestPacket packet)
{
   headless::SyncNewAddressRequest request;
   if (!request.ParseFromString(packet.data())) {
      logger_->error("[{}] failed to parse request", __func__);
      return false;
   }
   const auto wallet = walletsMgr_->getWalletById(request.wallet_id());
   if (wallet == nullptr) {
      logger_->error("[{}] wallet with ID {} not found", __func__, request.wallet_id());
      return false;
   }

   headless::ExtendAddressChainResponse response;
   response.set_wallet_id(wallet->walletId());

   for (int i = 0; i < request.addresses_size(); ++i) {
      const auto inData = request.addresses(i);
      auto outData = response.add_addresses();
      const auto addr = wallet->synchronizeUsedAddressChain(inData.index()).first.display();
      outData->set_address(addr);
      outData->set_index(inData.index());
   }

   if (callbacks_) {
      callbacks_->walletChanged(wallet->walletId());
   }

   packet.set_data(response.SerializeAsString());
   sendData(packet.SerializeAsString(), clientId);
   return true;
}

bool HeadlessContainerListener::onExecCustomDialog(const std::string &clientId, headless::RequestPacket packet)
{
   headless::CustomDialogRequest request;
   if (!request.ParseFromString(packet.data())) {
      logger_->error("[HeadlessContainerListener] failed to parse CustomDialogRequest");
      return false;
   }

   if (callbacks_) {
      callbacks_->customDialog(request.dialogname(), request.variantdata());
   }
   return true;
}

bool PasswordRequest::operator <(const PasswordRequest &other) const {
   seconds thisInterval, otherInterval;

   thisInterval = seconds(dialogData.value<int>("Duration"));
   if (thisInterval == 0s) {
      thisInterval = kDefaultDuration;
   }

   otherInterval = seconds(other.dialogData.value<int>("Duration"));
   if (otherInterval == 0s) {
      otherInterval = kDefaultDuration;
   }

   return dialogRequestedTime + thisInterval < other.dialogRequestedTime + otherInterval;
}
