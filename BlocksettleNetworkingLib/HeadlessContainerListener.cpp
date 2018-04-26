#include "HeadlessContainerListener.h"
#include <spdlog/spdlog.h>
#include "CheckRecipSigner.h"
#include "ConnectionManager.h"
#include "HDNode.h"
#include "HDWallet.h"
#include "WalletsManager.h"
#include "ZmqSecuredServerConnection.h"


using namespace Blocksettle::Communication;

HeadlessContainerListener::HeadlessContainerListener(const std::shared_ptr<ServerConnection> &conn
   , const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<WalletsManager> &walletsMgr
   , const std::string &pwHash, bool hasUI)
   : QObject(nullptr), ServerConnectionListener()
   , connection_(conn)
   , logger_(logger)
   , walletsMgr_(walletsMgr)
   , pwHash_(pwHash)
   , hasUI_(hasUI)
{
   connect(this, &HeadlessContainerListener::xbtSpent, this, &HeadlessContainerListener::onXbtSpent);
}

HeadlessContainerListener::~HeadlessContainerListener()
{
   if (!connection_) {
      return;
   }
   disconnect();
}

bool HeadlessContainerListener::disconnect(const std::string &clientId)
{
   headless::RequestPacket packet;
   packet.set_data("");
   packet.set_authticket(authTicket_.toBinStr());
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

void HeadlessContainerListener::SetLimits(const SignContainer::Limits &limits)
{
   limits_ = limits;
}

void HeadlessContainerListener::OnClientConnected(const std::string &clientId)
{
   logger_->debug("[HeadlessContainerListener] client {} connected", clientId);
}

void HeadlessContainerListener::OnClientDisconnected(const std::string &clientId)
{
   logger_->debug("[HeadlessContainerListener] client {} disconnected", clientId);
   emit clientDisconnected(clientId);
}

void HeadlessContainerListener::OnDataFromClient(const std::string &clientId, const std::string &data)
{
   headless::RequestPacket packet;
   if (!packet.ParseFromString(data)) {
      logger_->error("[HeadlessContainerListener] failed to parse request packet");
      return;
   }

   if ((packet.type() != headless::AuthenticationRequestType)
      && (authTicket_.isNull() || (SecureBinaryData(packet.authticket()) != authTicket_))) {
      logger_->error("[HeadlessContainerListener] auth ticket mismatch!");
      disconnect(clientId);
      return;
   }

   if (packet.type() == headless::AuthenticationRequestType) {
      if (!authTicket_.isNull()) {
         logger_->warn("[HeadlessContainerListener] already authenticated");
//         AuthResponse(clientId, packet, "already authenticated");
      }

      headless::AuthenticationRequest request;
      if (!request.ParseFromString(packet.data())) {
         logger_->error("[HeadlessContainerListener] failed to parse auth request");
         AuthResponse(clientId, packet, "failed to parse request");
         return;
      }
      if (!pwHash_.empty() && (pwHash_ != request.password())) {
         logger_->error("[HeadlessContainerListener] wrong auth password");
         AuthResponse(clientId, packet, "wrong pasasword");
         return;
      }

      authTicket_ = SecureBinaryData().GenerateRandom(8);
      AuthResponse(clientId, packet);
   }
   else {
      onRequestPacket(clientId, packet);
   }
}

void HeadlessContainerListener::AuthResponse(const std::string &clientId, headless::RequestPacket packet
   , const std::string &errMsg)
{
   headless::AuthenticationReply response;
   if (errMsg.empty()) {
      response.set_authticket(authTicket_.toBinStr());
      if (hasUI_) {
         response.set_hasui(true);
      }
   }
   else {
      response.set_error(errMsg);
   }
   packet.set_data(response.SerializeAsString());
   if (!sendData(packet.SerializeAsString(), clientId)) {
      logger_->error("[HeadlessContainerListener] failed to send response auth packet");
      return;
   }
   logger_->info("[HeadlessContainerListener] client {} authenticated", clientId);
   connectedClients_.insert(clientId);
   emit clientAuthenticated(clientId, connection_->GetClientInfo(clientId));
}

bool HeadlessContainerListener::onRequestPacket(const std::string &clientId, headless::RequestPacket packet)
{
   switch (packet.type()) {
   case headless::HeartbeatType:
      packet.set_data({});
      if (!sendData(packet.SerializeAsString(), clientId)) {
         logger_->error("[HeadlessContainerListener] failed to send response hearbeat packet");
         return false;
      }
      break;

   case headless::SignTXRequestType:
      return onSignTXRequest(clientId, packet);

   case headless::SignPartialTXRequestType:
      return onSignTXRequest(clientId, packet, true);

   case headless::SignPayoutTXRequestType:
      return onSignPayoutTXRequest(clientId, packet);

   case headless::SignTXMultiRequestType:
      return onSignMultiTXRequest(clientId, packet);

   case headless::PasswordRequestType:
      return onPasswordReceived(packet);

   case headless::SetUserIdRequestType:
      return onSetUserId(clientId, packet);

   case headless::SyncAddressRequestType:
      return onSyncAddress(clientId, packet);

   case headless::CreateHDWalletRequestType:
      return onCreateHDWallet(clientId, packet);

   case headless::DeleteHDWalletRequestType:
      return onDeleteHDWallet(packet);

   case headless::GetRootKeyRequestType:
      return onGetRootKey(clientId, packet);

   case headless::SetLimitsRequestType:
      return onSetLimits(clientId, packet);

   case headless::GetHDWalletInfoRequestType:
      return onGetHDWalletInfo(clientId, packet);

   case headless::ChangePasswordRequestType:
      return onChangePassword(clientId, packet);

   case headless::DisconnectionRequestType:
      emit OnClientDisconnected(clientId);
      break;

   default:
      logger_->error("[HeadlessContainerListener] unknown request type {}", packet.type());
      return false;
   }
   return true;
}

bool HeadlessContainerListener::onSignTXRequest(const std::string &clientId, const headless::RequestPacket &packet, bool partial)
{
   const auto reqType = partial ? headless::SignPartialTXRequestType : headless::SignTXRequestType;
   headless::SignTXRequest request;
   if (!request.ParseFromString(packet.data())) {
      logger_->error("[HeadlessContainerListener] failed to parse SignTXRequest");
      SignTXResponse(clientId, packet.id(), reqType, "failed to parse");
      return false;
   }
   uint64_t inputVal = 0;
   bs::wallet::TXSignRequest txSignReq;
   txSignReq.walletId = request.walletid();
   for (int i = 0; i < request.inputs_size(); i++) {
      UTXO utxo;
      utxo.unserialize(request.inputs(i));
      if (utxo.isInitialized()) {
         txSignReq.inputs.push_back(utxo);
         inputVal += utxo.getValue();
      }
   }

   uint64_t outputVal = 0;
   for (int i = 0; i < request.recipients_size(); i++) {
      BinaryData serialized = request.recipients(i);
      const auto recip = ScriptRecipient::deserialize(serialized);
      txSignReq.recipients.push_back(recip);
      const auto outAddr = bs::Address::fromRecipient(recip);
      outputVal += recip->getValue();
   }
   uint64_t value = outputVal;

   txSignReq.fee = request.fee();
   txSignReq.RBF = request.rbf();

   if (!request.unsignedstate().empty()) {
      const BinaryData prevState(request.unsignedstate());
      txSignReq.prevStates.push_back(prevState);
      if (!value) {
         bs::CheckRecipSigner signer(prevState);
         value = signer.spendValue();
         if (txSignReq.change.value) {
            value -= txSignReq.change.value;
         }
      }
   }

   if (request.has_change()) {
      txSignReq.change.address = request.change().address();
      txSignReq.change.index = request.change().index();
      txSignReq.change.value = request.change().value();
   }

   if (!txSignReq.isValid()) {
      logger_->error("[HeadlessContainerListener] invalid SignTXRequest");
      SignTXResponse(clientId, packet.id(), reqType, "missing critical data");
      return false;
   }

   txSignReq.populateUTXOs = request.populateutxos();

   const auto wallet = walletsMgr_->GetWalletById(txSignReq.walletId);
   if (!wallet) {
      logger_->error("[HeadlessContainerListener] failed to find wallet {}", txSignReq.walletId);
      SignTXResponse(clientId, packet.id(), reqType, "failed to find wallet " + txSignReq.walletId);
      return false;
   }
   const auto rootWalletId = walletsMgr_->GetHDRootForLeaf(txSignReq.walletId)->getWalletId();

   if ((wallet->GetType() == bs::wallet::Type::Bitcoin)
      && !CheckSpendLimit(value, request.applyautosignrules(), rootWalletId)) {
      SignTXResponse(clientId, packet.id(), reqType, "spend limit exceeded");
      return false;
   }

   const auto onPassword = [this, wallet, txSignReq, rootWalletId, clientId, id = packet.id(), partial
      , reqType, value, autoSign = request.applyautosignrules()] (const std::string &pass) {
      try {
         if (wallet->isEncrypted() && pass.empty()) {
            logger_->error("[HeadlessContainerListener] empty password for wallet {}", wallet->GetWalletName());
            SignTXResponse(clientId, id, reqType, "missing password for encrypted wallet");
            return;
         }
         const auto tx = partial ? wallet->SignPartialTXRequest(txSignReq, pass)
            : wallet->SignTXRequest(txSignReq, pass);
         SignTXResponse(clientId, id, reqType, {}, tx);
         emit xbtSpent(value, autoSign);
      }
      catch (const std::exception &e) {
         logger_->error("[HeadlessContainerListener] failed to sign {} TX request: {}", partial ? "partial" : "full", e.what());
         SignTXResponse(clientId, id, reqType, std::string("failed to sign: ") + e.what());
         passwords_.erase(wallet->GetWalletId());
         passwords_.erase(rootWalletId);
         emit autoSignDeactivated(rootWalletId);
      }
   };

   if (!request.password().empty()) {
      onPassword(request.password());
      return true;
   }

   const QString prompt = tr("Signing %1transaction").arg(partial ? tr("partial ") : tr(""));
   return RequestPasswordIfNeeded(clientId, txSignReq, prompt, onPassword, request.applyautosignrules());
}

bool HeadlessContainerListener::onSignPayoutTXRequest(const std::string &clientId, const headless::RequestPacket &packet)
{
   const auto reqType = headless::SignPayoutTXRequestType;
   headless::SignPayoutTXRequest request;
   if (!request.ParseFromString(packet.data())) {
      logger_->error("[HeadlessContainerListener] failed to parse SignPayoutTXRequest");
      SignTXResponse(clientId, packet.id(), reqType, "failed to parse");
      return false;
   }

   const auto &settlWallet = walletsMgr_->GetSettlementWallet();
   if (!settlWallet) {
      logger_->error("[HeadlessContainerListener] Settlement wallet is missing");
      SignTXResponse(clientId, packet.id(), reqType, "no settlement wallet");
      return false;
   }

   const auto &authWallet = walletsMgr_->GetAuthWallet();
   if (!authWallet) {
      logger_->error("[HeadlessContainerListener] Auth wallet is missing");
      SignTXResponse(clientId, packet.id(), reqType, "no auth wallet");
      return false;
   }

   bs::wallet::TXSignRequest txSignReq;
   txSignReq.walletId = authWallet->GetWalletId();
   UTXO utxo;
   utxo.unserialize(request.input());
   if (utxo.isInitialized()) {
      txSignReq.inputs.push_back(utxo);
   }

   BinaryData serialized = request.recipient();
   const auto recip = ScriptRecipient::deserialize(serialized);
   txSignReq.recipients.push_back(recip);

   const bs::Address authAddr(request.authaddress());
   const BinaryData settlementId = request.settlementid();
   const BinaryData buyAuthKey = request.buyauthkey();
   const BinaryData sellAuthKey = request.sellauthkey();

   const auto rootWalletId = walletsMgr_->GetHDRootForLeaf(authWallet->GetWalletId())->getWalletId();

   const auto onAuthPassword = [this, clientId, id = packet.id(), txSignReq, authWallet, authAddr
      , settlWallet, settlementId, buyAuthKey, sellAuthKey, reqType, rootWalletId](const std::string &pass) {
      if (authWallet->isEncrypted() && pass.empty()) {
         logger_->error("[HeadlessContainerListener] no password for encrypted auth wallet");
         SignTXResponse(clientId, id, reqType, "password required, but empty received");
      }

      const auto authKeys = authWallet->GetKeyPairFor(authAddr, pass);
      if (authKeys.privKey.isNull() || authKeys.pubKey.isNull()) {
         logger_->error("[HeadlessContainerListener] failed to get priv/pub keys for {}", authAddr.display<std::string>());
         SignTXResponse(clientId, id, reqType, "no auth priv/pub keys found");
         passwords_.erase(authWallet->GetWalletId());
         passwords_.erase(rootWalletId);
         emit autoSignDeactivated(rootWalletId);
         return;
      }

      const auto onSettlPassword = [this, clientId, id, txSignReq, authKeys, settlWallet, settlementId
         , buyAuthKey, sellAuthKey, reqType](const std::string &pass) {
         try {
            const auto tx = settlWallet->SignPayoutTXRequest(txSignReq, authKeys, settlementId, buyAuthKey, sellAuthKey);
            SignTXResponse(clientId, id, reqType, {}, tx);
         }
         catch (const std::exception &e) {
            logger_->error("[HeadlessContainerListener] failed to sign PayoutTX request: {}", e.what());
            SignTXResponse(clientId, id, reqType, std::string("failed to sign: ") + e.what());
         }
      };
      onSettlPassword({});
   };

   if (!request.password().empty()) {
      onAuthPassword(request.password());
      return true;
   }

   const QString prompt = tr("Signing pay-out transaction for %1 XBT:\n"
      "  Settlement ID: %2\n"
      "  Buyer's authentication key: %3\n"
      "  Seller's authentication key: %4"
   ).arg(QString::number(utxo.getValue() / BTCNumericTypes::BalanceDivider, 'f', 8))
      .arg(QString::fromStdString(settlementId.toHexStr()))
      .arg(QString::fromStdString(buyAuthKey.toHexStr())).arg(QString::fromStdString(sellAuthKey.toHexStr()));

   return RequestPasswordIfNeeded(clientId, txSignReq, prompt, onAuthPassword, request.applyautosignrules());
}

bool HeadlessContainerListener::onSignMultiTXRequest(const std::string &clientId, const headless::RequestPacket &packet)
{
   const auto reqType = headless::SignTXMultiRequestType;
   headless::SignTXMultiRequest request;
   if (!request.ParseFromString(packet.data())) {
      logger_->error("[HeadlessContainerListener] failed to parse SignTXMultiRequest");
      SignTXResponse(clientId, packet.id(), reqType, "failed to parse");
      return false;
   }

   bs::wallet::TXMultiSignRequest txMultiReq;
   txMultiReq.prevState = request.signerstate();
   for (int i = 0; i < request.walletids_size(); i++) {
      const auto &wallet = walletsMgr_->GetWalletById(request.walletids(i));
      if (!wallet) {
         logger_->error("[HeadlessContainerListener] failed to find wallet with id {}", request.walletids(i));
         SignTXResponse(clientId, packet.id(), reqType, "failed to find wallet " + request.walletids(i));
         return false;
      }
      txMultiReq.wallets.push_back(wallet);
   }

   QString prompt = tr("Signing multi-wallet input (auth revoke) transaction");

   const auto cbOnAllPasswords = [this, txMultiReq, reqType, clientId, id=packet.id()]
                                 (const std::unordered_map<std::string, std::string> &walletPasswords) {
      const auto cbWalletPass = [walletPasswords, this](const std::string &walletId) -> SecureBinaryData {
         const auto passIt = walletPasswords.find(walletId);
         if (passIt == walletPasswords.end()) {
            return {};
         }
         return passIt->second;
      };

      try {
         const auto tx = bs::SignMultiInputTX(txMultiReq, cbWalletPass);
         SignTXResponse(clientId, id, reqType, {}, tx);
      }
      catch (const std::exception &e) {
         logger_->error("[HeadlessContainerListener] failed to sign multi TX request: {}", e.what());
         SignTXResponse(clientId, id, reqType, std::string("failed to sign: ") + e.what());
      }
   };
   return RequestPasswordsIfNeeded(++reqSeqNo_, clientId, txMultiReq, prompt, cbOnAllPasswords);
}

void HeadlessContainerListener::SignTXResponse(const std::string &clientId, unsigned int id, headless::RequestType reqType
   , const std::string &error, const BinaryData &tx)
{
   headless::SignTXReply response;
   if (tx.isNull()) {
      response.set_error(error);
   }
   else {
      response.set_signedtx(tx.toBinStr());
   }

   headless::RequestPacket packet;
   packet.set_id(id);
   packet.set_authticket(authTicket_.toBinStr());
   packet.set_type(reqType);
   packet.set_data(response.SerializeAsString());

   if (!sendData(packet.SerializeAsString(), clientId)) {
      logger_->error("[HeadlessContainerListener] failed to send response signTX packet");
   }
   if (!tx.isNull()) {
      emit txSigned();
   }
}

bool HeadlessContainerListener::onPasswordReceived(headless::RequestPacket &packet)
{
   headless::PasswordReply response;
   if (!response.ParseFromString(packet.data())) {
      logger_->error("[HeadlessContainerListener] failed to parse PasswordReply");
      return false;
   }
   if (response.walletid().empty()) {
      logger_->error("[HeadlessContainerListener] walletId is empty in PasswordReply");
      return false;
   }
   if (!response.password().empty()) {
      passwords_[response.walletid()] = response.password();
   }

   passwordReceived(response.walletid(), response.password());
   return true;
}

void HeadlessContainerListener::passwordReceived(const std::string &walletId, const std::string &password)
{
   const auto cbsIt = passwordCallbacks_.find(walletId);
   if (cbsIt != passwordCallbacks_.end()) {
      for (const auto &cb : cbsIt->second) {
         cb(password);
      }
      passwordCallbacks_.erase(cbsIt);
   }
   if (autoSignPwdReqs_.find(walletId) != autoSignPwdReqs_.end()) {
      autoSignPwdReqs_.erase(walletId);
      activateAutoSign(walletId, password);
   }
}

bool HeadlessContainerListener::RequestPasswordIfNeeded(const std::string &clientId, const bs::wallet::TXSignRequest &txReq
   , const QString &prompt, const PasswordReceivedCb &cb, bool autoSign)
{
   const auto &wallet = walletsMgr_->GetWalletById(txReq.walletId);
   if (!wallet) {
      return false;
   }
   bool needPassword = wallet->isEncrypted();
   std::string password;
   std::string walletId = wallet->GetWalletId();
   if (needPassword && autoSign) {
      const auto &hdRoot = walletsMgr_->GetHDRootForLeaf(walletId);
      if (hdRoot) {
         walletId = hdRoot->getWalletId();
      }
      const auto passwordIt = passwords_.find(walletId);
      if (passwordIt != passwords_.end()) {
         needPassword = false;
         password = passwordIt->second;
      }
   }
   if (!needPassword) {
      if (cb) {
         cb(password);
      }
      return true;
   }

   return RequestPassword(clientId, txReq, prompt, cb);
}

bool HeadlessContainerListener::RequestPasswordsIfNeeded(int reqId, const std::string &clientId
   , const bs::wallet::TXMultiSignRequest &txMultiReq, const QString &prompt, const PasswordsReceivedCb &cb)
{
   TempPasswords tempPasswords;
   for (const auto &wallet : txMultiReq.wallets) {
      const auto &walletId = wallet->GetWalletId();
      const auto &rootWallet = walletsMgr_->GetHDRootForLeaf(walletId);
      const auto &rootWalletId = rootWallet->getWalletId();

      tempPasswords.rootLeaves[rootWalletId].insert(walletId);
      tempPasswords.reqWalletIds.insert(walletId);

      if (rootWallet->isEncrypted()) {
         const auto cbWalletPass = [this, reqId, cb, rootWalletId](const std::string &password) {
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

         bs::wallet::TXSignRequest txReq;
         txReq.walletId = rootWallet->getWalletId();
         RequestPassword(clientId, txReq, prompt, cbWalletPass);
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

bool HeadlessContainerListener::RequestPassword(const std::string &clientId, const bs::wallet::TXSignRequest &txReq
   , const QString &prompt, const PasswordReceivedCb &cb)
{
   if (cb) {
      auto &callbacks = passwordCallbacks_[txReq.walletId];
      callbacks.push_back(cb);
      if (callbacks.size() > 1) {
         return true;
      }
   }

   if (hasUI_) {
      emit passwordRequired(txReq, prompt);
      return true;
   }
   else {
      headless::PasswordRequest request;
      if (!prompt.isEmpty()) {
         request.set_prompt(prompt.toStdString());
      }
      if (!txReq.walletId.empty()) {
         request.set_walletid(txReq.walletId);
      }

      headless::RequestPacket packet;
      packet.set_authticket(authTicket_.toBinStr());
      packet.set_type(headless::PasswordRequestType);
      packet.set_data(request.SerializeAsString());
      return sendData(packet.SerializeAsString(), clientId);
   }
}

bool HeadlessContainerListener::onSetUserId(const std::string &clientId, headless::RequestPacket &packet)
{
   headless::SetUserIdRequest request;
   if (!request.ParseFromString(packet.data())) {
      logger_->error("[HeadlessContainerListener] failed to parse SetUserIdRequest");
      return false;
   }

   connect(walletsMgr_.get(), &WalletsManager::authWalletChanged, [this, clientId] {
      headless::RequestPacket packet;
      packet.set_authticket(authTicket_.toBinStr());
      packet.set_type(headless::SetUserIdRequestType);
      sendData(packet.SerializeAsString(), clientId);
   });
   walletsMgr_->SetUserId(request.userid());
   return true;
}

static AddressEntryType mapAddressType(headless::AddressType addrType) {
   switch (addrType) {
   case headless::LegacyAddressType:   return AddressEntryType_P2PKH;
   case headless::NestedSWAddressType: return AddressEntryType_P2SH;
   case headless::NativeSWAddressType:
   default:                            return AddressEntryType_P2WPKH;
   }
}

bool HeadlessContainerListener::onSyncAddress(const std::string &clientId, headless::RequestPacket &packet)
{
   headless::SyncAddressRequest request;
   if (!request.ParseFromString(packet.data())) {
      logger_->error("[HeadlessContainerListener] failed to parse SyncAddressRequest");
      return false;
   }

   std::set<std::shared_ptr<bs::hd::Wallet>> walletsForBackup;
   std::vector<std::tuple<std::shared_ptr<bs::Wallet>, std::string, headless::AddressType>> newAddresses;
   std::set<std::string> failedWallets;
   bool rc = true;

   for (int i = 0; i < request.address_size(); i++) {
      const auto &walletId = request.address(i).walletid();
      const auto &wallet = walletsMgr_->GetWalletById(walletId);
      if (!wallet) {
         logger_->warn("[HeadlessContainerListener] failed to get wallet for id {}", walletId);
         failedWallets.insert(walletId);
         rc = false;
         continue;
      }
      const auto &index = request.address(i).index();
      if (index.empty()) {
         continue;
      }
      if (!wallet->AddressIndexExists(index)) {
         walletsForBackup.insert(walletsMgr_->GetHDRootForLeaf(walletId));
         newAddresses.push_back(std::tuple<std::shared_ptr<bs::Wallet>, std::string, headless::AddressType>{ wallet, index, request.address(i).addrtype() });
      }
   }
   for (const auto &hdWallet : walletsForBackup) {
      if (hdWallet) {
         walletsMgr_->BackupWallet(hdWallet);
      }
   }

   std::vector<std::pair<std::string, std::string>> failedAddresses;
   for (const auto &tuple : newAddresses) {
      const auto &wallet = std::get<0>(tuple);
      const auto &index = std::get<1>(tuple);
      if (index.empty()) {    // just checking for wallet existence
         continue;
      }
      const auto addr = wallet->CreateAddressWithIndex(index, mapAddressType(std::get<2>(tuple)));
      if (addr.isNull()) {
         logger_->warn("[HeadlessContainerListener] failed to create address for index {}", index);
         rc = false;
         failedAddresses.push_back({wallet->GetWalletId(), index});
      }
      else {
         logger_->debug("[HeadlessContainerListener] created address {} for index {}"
            , addr.display<std::string>(), index);
      }
   }
   SyncAddrResponse(clientId, packet.id(), failedWallets, failedAddresses);
   return rc;
}

void HeadlessContainerListener::SyncAddrResponse(const std::string &clientId, unsigned int id
   , const std::set<std::string> &failedWallets, const std::vector<std::pair<std::string, std::string>> &failedAddresses)
{
   headless::SyncAddressResponse response;
   for (const auto &walletId : failedWallets) {
      response.add_missingwalletid(walletId);
   }
   for (const auto &addr : failedAddresses) {
      auto failedAddr = response.add_failedaddress();
      failedAddr->set_walletid(addr.first);
      failedAddr->set_index(addr.second);
   }

   headless::RequestPacket packet;
   packet.set_id(id);
   packet.set_authticket(authTicket_.toBinStr());
   packet.set_type(headless::SyncAddressRequestType);
   packet.set_data(response.SerializeAsString());

   if (!sendData(packet.SerializeAsString(), clientId)) {
      logger_->error("[HeadlessContainerListener] failed to send response SyncAddress packet");
   }
}

bool HeadlessContainerListener::CreateHDLeaf(const std::string &clientId, unsigned int id, const headless::NewHDLeaf &request
   , const std::string &password)
{
   const auto hdWallet = walletsMgr_->GetHDWalletById(request.rootwalletid());
   if (!hdWallet) {
      logger_->error("[HeadlessContainerListener] failed to find root HD wallet by id {}", request.rootwalletid());
      CreateHDWalletResponse(clientId, id, "no root HD wallet");
      return false;
   }
   const auto path = bs::hd::Path::fromString(request.path());
   if ((path.length() != 3) || !path.isAbolute()) {
      logger_->error("[HeadlessContainerListener] invalid path {} at HD wallet creation", request.path());
      CreateHDWalletResponse(clientId, id, "invalid path");
      return false;
   }

   walletsMgr_->BackupWallet(hdWallet);

   const auto onPassword = [this, hdWallet, path, clientId, id](const std::string &pass) {
      std::shared_ptr<bs::hd::Node> leafNode;
      if (hdWallet->isEncrypted() && pass.empty()) {
         logger_->error("[HeadlessContainerListener] no password for encrypted wallet");
         CreateHDWalletResponse(clientId, id, "password required, but empty received");
      }
      if (hdWallet->isEncrypted()) {
         const auto decrypted = hdWallet->getNode()->decrypt(pass);
         if (decrypted) {
            leafNode = decrypted->derive(path);
         }
         else {
            logger_->error("[HeadlessContainerListener] failed to decrypt root node");
            CreateHDWalletResponse(clientId, id, "root node decryption failed");
         }
      }
      else {
         leafNode = hdWallet->getNode()->derive(path);
      }

      if (leafNode) {
         leafNode->clearPrivKey();

         const auto groupIndex = static_cast<bs::hd::CoinType>(path.get(1));
         auto group = hdWallet->getGroup(groupIndex);
         if (!group) {
            group = hdWallet->createGroup(groupIndex);
         }
         const auto leafIndex = path.get(2);
         auto leaf = group->createLeaf(leafIndex, leafNode);
         if (!leaf && !(leaf = group->getLeaf(leafIndex))) {
            logger_->error("[HeadlessContainerListener] failed to create/get leaf {}", path.toString());
         }
         if (!leaf->isInitialized()) {
            logger_->warn("[HeadlessContainerListener] leaf {} is not inited", path.toString());
         }

         CreateHDWalletResponse(clientId, id, leaf ? leaf->GetWalletId() : std::string{}
         , leafNode->pubCompressedKey(), leafNode->chainCode());
      }
      else {
         logger_->error("[HeadlessContainerListener] failed to create HD leaf");
         CreateHDWalletResponse(clientId, id, "failed to derive");
      }
   };

   if (hdWallet->isEncrypted()) {
      if (!password.empty()) {
         onPassword(password);
      }
      else {
         bs::wallet::TXSignRequest txReq;
         txReq.walletId = hdWallet->getWalletId();
         return RequestPassword(clientId, txReq, tr("Creating a wallet %1").arg(QString::fromStdString(txReq.walletId)), onPassword);
      }
   }
   else {
      onPassword({});
   }
   return true;
}

bool HeadlessContainerListener::CreateHDWallet(const std::string &clientId, unsigned int id, const headless::NewHDWallet &request
   , const std::string &password)
{
   std::shared_ptr<bs::hd::Wallet> wallet;
   try {
      const auto netType = walletsMgr_->GetNetworkType();
      const auto seed = request.privatekey().empty() ? bs::wallet::Seed(request.seed(), netType)
         : bs::wallet::Seed(netType, request.privatekey());
      wallet = walletsMgr_->CreateWallet(request.name(), request.description()
         , password, request.primary(), seed);
   }
   catch (const std::exception &e) {
      CreateHDWalletResponse(clientId, id, e.what());
      return false;
   }
   if (!wallet) {
      CreateHDWalletResponse(clientId, id, "creation failed");
      return false;
   }
   try {
      const auto woWallet = wallet->CreateWatchingOnly(password);
      if (!woWallet) {
         CreateHDWalletResponse(clientId, id, "failed to create watching-only copy");
         return false;
      }
      CreateHDWalletResponse(clientId, id, {}, {}, {}, woWallet);
   }
   catch (const std::exception &e) {
      CreateHDWalletResponse(clientId, id, e.what());
      return false;
   }
   return true;
}

bool HeadlessContainerListener::onCreateHDWallet(const std::string &clientId, headless::RequestPacket &packet)
{
   headless::CreateHDWalletRequest request;
   if (!request.ParseFromString(packet.data())) {
      logger_->error("[HeadlessContainerListener] failed to parse CreateHDWalletRequest");
      CreateHDWalletResponse(clientId, packet.id(), "failed to parse");
      return false;
   }
   if (request.has_leaf()) {
      return CreateHDLeaf(clientId, packet.id(), request.leaf(), request.password());
   }
   else if (request.has_wallet()) {
      return CreateHDWallet(clientId, packet.id(), request.wallet(), request.password());
   }
   else {
      CreateHDWalletResponse(clientId, packet.id(), "unknown request");
   }
   return false;
}

void HeadlessContainerListener::CreateHDWalletResponse(const std::string &clientId, unsigned int id, const std::string &errorOrWalletId
   , const BinaryData &pubKey, const BinaryData &chainCode, const std::shared_ptr<bs::hd::Wallet> &wallet)
{
   headless::CreateHDWalletResponse response;
   if (!pubKey.isNull() && !chainCode.isNull()) {
      auto leaf = response.mutable_leaf();
      leaf->set_pubkey(pubKey.toBinStr());
      leaf->set_chaincode(chainCode.toBinStr());
      leaf->set_walletid(errorOrWalletId);
   }
   else if (wallet) {
      auto wlt = response.mutable_wallet();
      wlt->set_name(wallet->getName());
      wlt->set_description(wallet->getDesc());
      wlt->set_walletid(wallet->getWalletId());
      wlt->set_nettype((wallet->getXBTGroupType() == bs::hd::CoinType::Bitcoin_test) ? headless::TestNetType : headless::MainNetType);
      for (const auto &group : wallet->getGroups()) {
         auto grp = wlt->add_groups();
         grp->set_path(group->getPath().toString());
         grp->set_name(group->getName());
         for (const auto &leaf : group->getLeaves()) {
            auto wLeaf = wlt->add_leaves();
            wLeaf->set_path(leaf->path().toString());
            wLeaf->set_walletid(leaf->GetWalletId());
            wLeaf->set_pubkey(leaf->getPubKey().toBinStr());
            wLeaf->set_chaincode(leaf->getChainCode().toBinStr());
         }
      }
   }
   else {
      response.set_error(errorOrWalletId);
   }

   headless::RequestPacket packet;
   packet.set_id(id);
   packet.set_authticket(authTicket_.toBinStr());
   packet.set_type(headless::CreateHDWalletRequestType);
   packet.set_data(response.SerializeAsString());

   if (!sendData(packet.SerializeAsString(), clientId)) {
      logger_->error("[HeadlessContainerListener] failed to send response CreateHDWallet packet");
   }
}

bool HeadlessContainerListener::onDeleteHDWallet(headless::RequestPacket &packet)
{
   headless::DeleteHDWalletRequest request;
   if (!request.ParseFromString(packet.data())) {
      logger_->error("[HeadlessContainerListener] failed to parse CreateHDWalletRequest");
      return false;
   }
   if (!request.leafwalletid().empty()) {
      const auto &walletId = request.leafwalletid();
      const auto &wallet = walletsMgr_->GetWalletById(walletId);
      if (!wallet) {
         logger_->error("[HeadlessContainerListener] failed to find leaf by id {}", walletId);
         return false;
      }
      logger_->debug("Deleting HDLeaf {}: {}", walletId, wallet->GetWalletName());
      return walletsMgr_->DeleteWalletFile(wallet);
   }
   else if (!request.rootwalletid().empty()) {
      const auto &walletId = request.rootwalletid();
      const auto &wallet = walletsMgr_->GetHDWalletById(walletId);
      if (!wallet) {
         logger_->error("[HeadlessContainerListener] failed to find HD Wallet by id {}", walletId);
         return false;
      }
      logger_->debug("Deleting HDWallet {}: {}", walletId, wallet->getName());
      return walletsMgr_->DeleteWalletFile(wallet);
   }

   logger_->error("[HeadlessContainerListener] can't delete any wallet type - no id specified");
   return false;
}

bool HeadlessContainerListener::onSetLimits(const std::string &clientId, headless::RequestPacket &packet)
{
   headless::SetLimitsRequest request;
   if (!request.ParseFromString(packet.data())) {
      logger_->error("[HeadlessContainerListener] failed to parse SetLimitsRequest");
      AutoSignActiveResponse({}, false, "request parse error", clientId, packet.id());
      return false;
   }
   if (request.rootwalletid().empty()) {
      logger_->error("[HeadlessContainerListener] no wallet specified in SetLimitsRequest");
      AutoSignActiveResponse(request.rootwalletid(), false, "invalid request", clientId, packet.id());
      return false;
   }
   if (!request.activateautosign()) {
      deactivateAutoSign(request.rootwalletid());
      return true;
   }

   if (!request.password().empty()) {
      activateAutoSign(request.rootwalletid(), request.password());
   }
   else {
      const auto &wallet = walletsMgr_->GetHDWalletById(request.rootwalletid());
      if (!wallet) {
         logger_->error("[HeadlessContainerListener] failed to find root wallet by id {} (to activate auto-sign)"
            , request.rootwalletid());
         AutoSignActiveResponse(request.rootwalletid(), false, "missing wallet", clientId, packet.id());
         return false;
      }
      if (wallet->isEncrypted() && !isAutoSignActive(request.rootwalletid())) {
         addPendingAutoSignReq(request.rootwalletid());
         emit autoSignRequiresPwd(request.rootwalletid());
      }
      else {
         emit autoSignActivated(request.rootwalletid());
         AutoSignActiveResponse(request.rootwalletid(), true, {}, clientId, packet.id());
      }
   }
   return true;
}

bool HeadlessContainerListener::onGetRootKey(const std::string &clientId, headless::RequestPacket &packet)
{
   headless::GetRootKeyRequest request;
   if (!request.ParseFromString(packet.data())) {
      logger_->error("[HeadlessContainerListener] failed to parse GetRootKeyRequest");
      GetRootKeyResponse(clientId, packet.id(), nullptr, "failed to parse request");
      return false;
   }
   const auto &wallet = walletsMgr_->GetHDWalletById(request.rootwalletid());
   if (!wallet) {
      logger_->error("[HeadlessContainerListener] failed to find wallet for id {}", request.rootwalletid());
      GetRootKeyResponse(clientId, packet.id(), nullptr, "failed to find wallet");
      return false;
   }
   if (wallet->isEncrypted() && request.password().empty()) {
      logger_->error("[HeadlessContainerListener] password is missing for encrypted wallet {}", request.rootwalletid());
      GetRootKeyResponse(clientId, packet.id(), nullptr, "password missing");
      return false;
   }

   logger_->info("Requested private key for wallet {}", request.rootwalletid());
   std::shared_ptr<bs::hd::Node> decrypted;
   if (wallet->isEncrypted()) {
      decrypted = wallet->getNode()->decrypt(request.password());
      bs::wallet::Seed seed(decrypted->getNetworkType(), decrypted->privateKey());
      if (bs::hd::Wallet(wallet->getName(), wallet->getDesc(), false, seed).getWalletId() != wallet->getWalletId()) {
         logger_->error("[HeadlessContainerListener] invalid password for {}", request.rootwalletid());
         GetRootKeyResponse(clientId, packet.id(), nullptr, "invalid password");
         return false;
      }
   }
   else {
      decrypted = wallet->getNode();
   }
   if (!decrypted) {
      logger_->error("[HeadlessContainerListener] failed to get/decrypt root node for {}", request.rootwalletid());
      GetRootKeyResponse(clientId, packet.id(), nullptr, "failed to get node");
      return false;
   }
   GetRootKeyResponse(clientId, packet.id(), decrypted, wallet->getWalletId());
   return true;
}

void HeadlessContainerListener::GetRootKeyResponse(const std::string &clientId, unsigned int id
   , const std::shared_ptr<bs::hd::Node> &decrypted, const std::string &errorOrId)
{
   headless::GetRootKeyResponse response;
   if (decrypted) {
      response.set_decryptedprivkey(decrypted->privateKey().toBinStr());
      response.set_chaincode(decrypted->chainCode().toBinStr());
   }
   response.set_walletid(errorOrId);

   headless::RequestPacket packet;
   packet.set_id(id);
   packet.set_authticket(authTicket_.toBinStr());
   packet.set_type(headless::GetRootKeyRequestType);
   packet.set_data(response.SerializeAsString());

   if (!sendData(packet.SerializeAsString(), clientId)) {
      logger_->error("[HeadlessContainerListener] failed to send response GetRootKey packet");
   }
}

bool HeadlessContainerListener::onGetHDWalletInfo(const std::string &clientId, headless::RequestPacket &packet)
{
   headless::GetHDWalletInfoRequest request;
   if (!request.ParseFromString(packet.data())) {
      logger_->error("[HeadlessContainerListener] failed to parse GetHDWalletInfoRequest");
      GetHDWalletInfoResponse(clientId, packet.id(), false, "failed to parse request");
      return false;
   }
   const auto &wallet = walletsMgr_->GetHDWalletById(request.rootwalletid());
   if (!wallet) {
      logger_->error("[HeadlessContainerListener] failed to find wallet for id {}", request.rootwalletid());
      GetHDWalletInfoResponse(clientId, packet.id(), false, "failed to find wallet");
      return false;
   }
   GetHDWalletInfoResponse(clientId, packet.id(), wallet->isEncrypted());
   return true;
}

void HeadlessContainerListener::GetHDWalletInfoResponse(const std::string &clientId, unsigned int id, bool encrypted
   , const std::string &error)
{
   headless::GetHDWalletInfoResponse response;
   if (!error.empty()) {
      response.set_error(error);
   }
   else {
      response.set_encrypted(encrypted);
   }

   headless::RequestPacket packet;
   packet.set_id(id);
   packet.set_authticket(authTicket_.toBinStr());
   packet.set_type(headless::GetHDWalletInfoRequestType);
   packet.set_data(response.SerializeAsString());

   if (!sendData(packet.SerializeAsString(), clientId)) {
      logger_->error("[HeadlessContainerListener] failed to send response GetHDWalletInfo packet");
   }
}

bool HeadlessContainerListener::onChangePassword(const std::string &clientId, headless::RequestPacket &packet)
{
   headless::ChangePasswordRequest request;
   if (!request.ParseFromString(packet.data())) {
      logger_->error("[HeadlessContainerListener] failed to parse GetHDWalletInfoRequest");
      ChangePasswordResponse(clientId, packet.id(), {}, false);
      return false;
   }
   const auto &wallet = walletsMgr_->GetHDWalletById(request.rootwalletid());
   if (!wallet) {
      logger_->error("[HeadlessContainerListener] failed to find wallet for id {}", request.rootwalletid());
      ChangePasswordResponse(clientId, packet.id(), request.rootwalletid(), false);
      return false;
   }
   if (!wallet->changePassword(request.newpassword(), request.oldpassword())) {
      logger_->error("[HeadlessContainerListener] failed to change password for wallet {}", request.rootwalletid());
      ChangePasswordResponse(clientId, packet.id(), request.rootwalletid(), false);
      return false;
   }
   logger_->info("Changed password for wallet {} (id: {})", wallet->getName(), wallet->getWalletId());
   ChangePasswordResponse(clientId, packet.id(), request.rootwalletid(), true);
   return true;
}

void HeadlessContainerListener::ChangePasswordResponse(const std::string &clientId, unsigned int id
   , const std::string &walletId, bool ok)
{
   headless::ChangePasswordResponse response;
   response.set_rootwalletid(walletId);
   response.set_success(ok);

   headless::RequestPacket packet;
   packet.set_id(id);
   packet.set_authticket(authTicket_.toBinStr());
   packet.set_type(headless::ChangePasswordRequestType);
   packet.set_data(response.SerializeAsString());

   if (!sendData(packet.SerializeAsString(), clientId)) {
      logger_->error("[HeadlessContainerListener] failed to send ChangePassword response");
   }
}

void HeadlessContainerListener::AutoSignActiveResponse(const std::string &walletId, bool active
   , const std::string &error, const std::string &clientId, unsigned int id)
{
   headless::SetLimitsResponse response;
   response.set_rootwalletid(walletId);
   response.set_autosignactive(active);
   if (!error.empty()) {
      response.set_error(error);
   }

   headless::RequestPacket packet;
   packet.set_id(id);
   packet.set_authticket(authTicket_.toBinStr());
   packet.set_type(headless::SetLimitsRequestType);
   packet.set_data(response.SerializeAsString());

   if (!sendData(packet.SerializeAsString(), clientId)) {
      if (clientId.empty()) {
         logger_->warn("[HeadlessContainerListener] failed to multicast SetLimits response");
      }
      else {
         logger_->error("[HeadlessContainerListener] failed to send SetLimits response");
      }
   }
}

bool HeadlessContainerListener::CheckSpendLimit(uint64_t value, bool autoSign, const std::string &walletId)
{
   if (autoSign) {
      if (value > limits_.autoSignSpendXBT) {
         logger_->warn("[HeadlessContainerListener] requested auto-sign spend {} exceeds limit {}", value
            , limits_.autoSignSpendXBT);
         deactivateAutoSign(walletId, "spend limit reached");
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

void HeadlessContainerListener::onXbtSpent(qint64 value, bool autoSign)
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

void HeadlessContainerListener::activateAutoSign(const std::string &walletId, const std::string &password)
{
   const auto &wallet = walletId.empty() ? walletsMgr_->GetPrimaryWallet() : walletsMgr_->GetHDWalletById(walletId);
   if (!wallet) {
      deactivateAutoSign(walletId, "wallet missing");
      return;
   }
   if (wallet->isEncrypted()) {
      if (password.empty()) {
         deactivateAutoSign(walletId, "empty password");
         return;
      }
      const auto decrypted = wallet->getNode()->decrypt(password);
      bs::wallet::Seed seed(decrypted->getNetworkType(), decrypted->privateKey());
      if (bs::hd::Wallet(wallet->getName(), wallet->getDesc(), false, seed).getWalletId() != wallet->getWalletId()) {
         deactivateAutoSign(walletId, "invalid password");
         return;
      }
   }
   passwords_[wallet->getWalletId()] = password;
   emit autoSignActivated(wallet->getWalletId());
   AutoSignActiveResponse(wallet->getWalletId(), true);
}

void HeadlessContainerListener::deactivateAutoSign(const std::string &walletId, const std::string &reason)
{
   if (walletId.empty()) {
      passwords_.clear();
   }
   else {
      passwords_.erase(walletId);
   }
   emit autoSignDeactivated(walletId);
   AutoSignActiveResponse(walletId, false, reason);
}

bool HeadlessContainerListener::isAutoSignActive(const std::string &walletId) const
{
   if (walletId.empty()) {
      return !passwords_.empty();
   }
   return (passwords_.find(walletId) != passwords_.end());
}

void HeadlessContainerListener::addPendingAutoSignReq(const std::string &walletId)
{
   if (walletId.empty()) {
      autoSignPwdReqs_.insert(walletsMgr_->GetPrimaryWallet()->getWalletId());
   }
   else {
      autoSignPwdReqs_.insert(walletId);
   }
}
