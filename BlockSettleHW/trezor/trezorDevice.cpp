/*

***********************************************************************************
* Copyright (C) 2020 - 2021, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include <QDataStream>
#include <spdlog/spdlog.h>
#include "hwdevicemanager.h"
#include "trezorDevice.h"
#include "trezorClient.h"
#include "Wallets/ProtobufHeadlessUtils.h"
#include "CoreWallet.h"
#include "Wallets/SyncWalletsManager.h"
#include "Wallets/SyncHDWallet.h"

#include "headless.pb.h"

// Trezor interface (source - https://github.com/trezor/trezor-common/tree/master/protob)
#include "trezor/generated_proto/messages.pb.h"
#include "trezor/generated_proto/messages-management.pb.h"
#include "trezor/generated_proto/messages-common.pb.h"
#include "trezor/generated_proto/messages-bitcoin.pb.h"
#include <google/protobuf/util/json_util.h>

using namespace hw::trezor::messages;
using namespace bs::hww;

namespace {
   const auto kModel1 = "1";
   const auto kModelT = "T";

   const std::map<std::string, std::array<unsigned, 3>> kMinVersion = {
      { kModel1, { 1, 9, 1 } },
      { kModelT, { 2, 3, 1 } }
   };

   // Trezor package rule (source - https://github.com/trezor/trezord-go)
   int getMessageType(const google::protobuf::Message &msg)
   {
      const std::string typeName = "MessageType_" + msg.GetDescriptor()->name();
      return hw::trezor::messages::MessageType_descriptor()
         ->FindValueByName(typeName)
         ->number();
   }

   QByteArray packMessage(const google::protobuf::Message &msg)
   {
      auto msg_type = getMessageType(msg);
      std::string serialized_msg = msg.SerializeAsString();
      int length = static_cast<int>(serialized_msg.size());

      QByteArray packed;
      QDataStream stream(&packed, QIODevice::WriteOnly);

      stream.writeRawData(QByteArray::number(msg_type, 16).rightJustified(4, '0'), 4);
      stream.writeRawData(QByteArray::number(length, 16).rightJustified(8, '0'), 8);
      stream.writeRawData(QByteArray::fromStdString(serialized_msg).toHex()
         , 2 * static_cast<int>(serialized_msg.size()));

      return packed;
   }

   trezor::MessageData unpackMessage(const QByteArray& response)
   {
      QDataStream stream(response);
      trezor::MessageData ret;

      ret.type = QByteArray::fromHex(response.mid(0, 4).toHex()).toInt(nullptr, 16);
      ret.length = QByteArray::fromHex(response.mid(4, 8).toHex()).toInt(nullptr, 16);
      ret.message = QByteArray::fromHex(response.mid(12, 2 * ret.length)).toStdString();

      return ret;
   }

   std::string getJSONReadableMessage(const google::protobuf::Message &msg)
   {
      std::string output;
      google::protobuf::util::JsonPrintOptions options;
      options.add_whitespace = true;
      google::protobuf::util::MessageToJsonString(msg, &output, options);

      return output;
   }

   static const std::string kTesNetCoin = "Testnet";
}

TrezorDevice::TrezorDevice(const std::shared_ptr<spdlog::logger> &logger
   , const trezor::DeviceData& data, bool testNet, DeviceCallbacks* cb
   , const std::string& endpoint)
   : bs::WorkerPool(1, 1)
   , logger_(logger), data_(data), testNet_(testNet), cb_(cb), endpoint_(endpoint)
   , features_{std::make_shared<management::Features>()}
{}

TrezorDevice::~TrezorDevice() = default;

std::shared_ptr<bs::Worker> TrezorDevice::worker(const std::shared_ptr<InData>&)
{
   const std::vector<std::shared_ptr<Handler>> handlers{ std::make_shared<TrezorPostHandler>
      (logger_, endpoint_) };
   return std::make_shared<bs::WorkerImpl>(handlers);
}

void bs::hww::TrezorDevice::operationFailed(const std::string& reason)
{
   releaseConnection();
   cb_->operationFailed(features_->device_id(), reason);
}

void TrezorDevice::releaseConnection()
{
   auto releaseCallback = [this](const std::shared_ptr<bs::OutData>& data)
   {
      const auto& reply = std::static_pointer_cast<TrezorPostOut>(data);
      if (!reply || !reply->error.empty()) {
         logger_->error("[TrezorDevice::releaseConnection] network error: {}"
            , reply ? reply->error : "<empty>");
         return;
      }

      logger_->info("[TrezorClient] releaseConnection - Connection successfully released");

      state_ = trezor::State::Released;
      //emit deviceReleased();
   };
   auto inData = std::make_shared<TrezorPostIn>();
   inData->path = "/release/" + data_.sessionId;
   processQueued(inData, releaseCallback);
}

DeviceKey TrezorDevice::key() const
{
   std::string walletId;
   std::string status;
   if (!xpubRoot_.empty()) {
      walletId = bs::core::wallet::computeID(xpubRoot_).toBinStr();
   }
   else {
      status = "not inited";
   }
   return { features_->label(), features_->device_id(), features_->vendor()
      , walletId, status, type() };
}

DeviceType TrezorDevice::type() const
{
   return DeviceType::HWTrezor;
}

void TrezorDevice::init()
{
   logger_->debug("[TrezorDevice::init] start");
   management::Initialize message;
   message.set_session_id(data_.sessionId);
   const auto& cb = [this](const std::shared_ptr<bs::OutData>& data)
   {
      const auto& reply = std::static_pointer_cast<TrezorPostOut>(data);
      if (!reply || !reply->error.empty()) {
         logger_->error("[TrezorDevice::makeCall] network error: {}", reply ? reply->error : "<empty>");
         //emit operationFailed(QLatin1String("Network error"));
         reset();
         return;
      }
      state_ = trezor::State::Init;
      const auto& msg = unpackMessage(QByteArray::fromStdString(reply->response));
      handleMessage(msg);

      retrieveXPubRoot();
   };
   auto inData = std::make_shared<TrezorPostIn>();
   inData->path = "/call/" + data_.sessionId;
   inData->input = packMessage(message).toStdString();
   processQueued(inData, cb);
}

struct NoDataOut : public bs::OutData
{
   ~NoDataOut() override = default;
   MessageType msgType;
};

struct XPubOut : public bs::OutData
{
   ~XPubOut() override = default;
   std::string xpub;
};

struct TXOut : public bs::OutData
{
   ~TXOut() override = default;
   std::string signedTX;
};

void TrezorDevice::getPublicKey()
{
   awaitingWalletInfo_ = {};
   // General data
   awaitingWalletInfo_.type = bs::wallet::HardwareEncKey::WalletType::Trezor;
   awaitingWalletInfo_.label = features_->label();
   awaitingWalletInfo_.deviceId = features_->device_id();
   awaitingWalletInfo_.vendor = features_->vendor();
   awaitingWalletInfo_.xpubRoot = xpubRoot_.toBinStr();

   if (!isFirmwareSupported()) {
      logger_->warn("[TrezorDevice::getPublicKey] unsupported firmware. {}"
         , firmwareSupportedVersion());
      //TODO: invoke callback
      return;
   }

   const auto& cbLegacy = [this](const std::shared_ptr<bs::OutData>& data)
   {
      const auto& reply = std::dynamic_pointer_cast<XPubOut>(data);
      if (!reply) {
         logger_->error("[TrezorDevice::getPublicKey::legacy] invalid callback data");
         return;
      }
      awaitingWalletInfo_.xpubLegacy = reply->xpub;
      //TODO: invoke callback
   };

   const auto& cbNested = [this](const std::shared_ptr<bs::OutData>& data)
   {
      const auto& reply = std::dynamic_pointer_cast<XPubOut>(data);
      if (!reply) {
         logger_->error("[TrezorDevice::getPublicKey::nested] invalid callback data");
         return;
      }
      awaitingWalletInfo_.xpubNestedSegwit = reply->xpub;
   };

   const auto& cbNative = [this](const std::shared_ptr<bs::OutData> &data)
   {
      const auto& reply = std::dynamic_pointer_cast<XPubOut>(data);
      if (!reply) {
         logger_->error("[TrezorDevice::getPublicKey::native] invalid callback data");
         return;
      }
      awaitingWalletInfo_.xpubNativeSegwit = reply->xpub;
   };

   logger_->debug("[TrezorDevice::getPublicKey] start public keys from device {}"
      , features_->label());
   bitcoin::GetPublicKey message;
   for (const uint32_t add : getDerivationPath(testNet_, bs::hd::Purpose::Native)) {
      message.add_address_n(add);
   }
   if (testNet_) {
      message.set_coin_name(kTesNetCoin);
   }
   makeCall(message, cbNative);

   message.clear_address_n();
   for (const uint32_t add : getDerivationPath(testNet_, bs::hd::Purpose::Nested)) {
      message.add_address_n(add);
   }
   makeCall(message, cbNested);

   message.clear_address_n();
   for (const uint32_t add : getDerivationPath(testNet_, bs::hd::Purpose::NonSegWit)) {
      message.add_address_n(add);
   }
   makeCall(message, cbLegacy);
}

void TrezorDevice::setMatrixPin(const SecureBinaryData& pin)
{
   logger_->debug("[TrezorDevice::setMatrixPin] {}", data_.path);
   common::PinMatrixAck message;
   message.set_pin(pin.toBinStr());
   makeCall(message);
}

void TrezorDevice::setPassword(const SecureBinaryData& password, bool enterOnDevice)
{
   logger_->debug("[TrezorDevice::setPassword] {}", data_.path);
   common::PassphraseAck message;
   if (enterOnDevice) {
      message.set_on_device(true);
   } else {
      message.set_passphrase(password.toBinStr());
   }
   makeCall(message);
}

void TrezorDevice::cancel()
{
   logger_->debug("[TrezorDevice] cancel previous operation");
   management::Cancel message;
   makeCall(message);
   sendTxMessage(/*HWInfoStatus::kCancelledByUser*/"cancelled by user");
}

void TrezorDevice::clearSession()
{
   logger_->debug("[TrezorDevice] cancel previous operation");
   management::/*ClearSession*/EndSession message;
   makeCall(message);
}


void TrezorDevice::signTX(const bs::core::wallet::TXSignRequest &reqTX)
{
   currentTxSignReq_.reset(new bs::core::wallet::TXSignRequest(reqTX));
   logger_->debug("[TrezorDevice::signTX] specify init data to {}", features_->label());

   bitcoin::SignTx message;
   message.set_inputs_count(currentTxSignReq_->armorySigner_.getTxInCount());
   message.set_outputs_count(currentTxSignReq_->armorySigner_.getTxOutCount());
   if (testNet_) {
      message.set_coin_name(kTesNetCoin);
   }
   const auto& cb = [this, reqTX](const std::shared_ptr<bs::OutData>& data)
   {
      const auto& reply = std::dynamic_pointer_cast<TXOut>(data);
      if (!reply) {
         logger_->error("[TrezorDevice::signTX] invalid callback data");
         return;
      }
      // According to architecture, Trezor allow us to sign tx with incorrect 
      // passphrase, so let's check that the final tx is correct. In Ledger case
      // this situation is impossible, since the wallets with different passphrase will be treated
      // as different devices, which will be verified in sign part.
      try {
         std::map<BinaryData, std::map<unsigned, UTXO>> utxoMap;
         for (unsigned i = 0; i < reqTX.armorySigner_.getTxInCount(); i++) {
            const auto& utxo = reqTX.armorySigner_.getSpender(i)->getUtxo();
            auto& idMap = utxoMap[utxo.getTxHash()];
            idMap.emplace(utxo.getTxOutIndex(), utxo);
         }
         unsigned flags = SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_SEGWIT | SCRIPT_VERIFY_P2SH_SHA256;
         bool validSign = Armory::Signer::Signer::verify(SecureBinaryData::fromString(reply->signedTX)
            , utxoMap, flags, true).isValid();
         if (!validSign) {
            SPDLOG_LOGGER_ERROR(logger_, "sign verification failed");
            operationFailed("Signing failed. Please ensure you typed the correct passphrase.");
            return;
         }
      }
      catch (const std::exception& e) {
         SPDLOG_LOGGER_ERROR(logger_, "sign verification failed: {}", e.what());
         operationFailed("Signing failed. Please ensure you typed the correct passphrase.");
         return;
      }
      cb_->txSigned(SecureBinaryData::fromString(reply->signedTX));
   };
   makeCall(message, cb);
}

void TrezorDevice::retrieveXPubRoot()
{
   logger_->debug("[TrezorDevice::retrieveXPubRoot] start retrieving root public"
      " key from device {}", features_->label());
   bitcoin::GetPublicKey message;
   message.add_address_n(bs::hd::hardFlag);
   if (testNet_) {
      message.set_coin_name(kTesNetCoin);
   }

   const auto& saveXpubRoot = [this](const std::shared_ptr<bs::OutData>& data)
   {
      const auto& reply = std::dynamic_pointer_cast<XPubOut>(data);
      if (!reply) {
         logger_->error("[TrezorDevice::retrieveXPubRoot] invalid callback data");
         return;
      }
      xpubRoot_ = BinaryData::fromString(reply->xpub);
   };
   makeCall(message, saveXpubRoot);
}

void TrezorDevice::makeCall(const google::protobuf::Message &msg
   , const bs::WorkerPool::callback& cb)
{
   if (state_ == trezor::State::None) {
      init();
   }
   const auto& cbWrap = [this, cb](const std::shared_ptr<bs::OutData>& data)
   {
      const auto& reply = std::static_pointer_cast<TrezorPostOut>(data);
      if (!reply || !reply->error.empty()) {
         logger_->error("[TrezorDevice::makeCall] network error: {}", reply ? reply->error : "<empty>");
         //emit operationFailed(QLatin1String("Network error"));
         reset();
         return;
      }
      const auto& msg = unpackMessage(QByteArray::fromStdString(reply->response));
      handleMessage(msg, cb);
   };
   auto inData = std::make_shared<TrezorPostIn>();
   inData->path = "/call/" + data_.sessionId;
   inData->input = packMessage(msg).toStdString();
   processQueued(inData, cbWrap);
}

void TrezorDevice::handleMessage(const trezor::MessageData& data, const bs::WorkerPool::callback& cb)
{
   switch (static_cast<MessageType>(data.type)) {
   case MessageType_Success:
      {
         common::Success success;
         parseResponse(success, data);
      }
      break;
   case MessageType_Failure:
      {
         state_ = trezor::State::None;
         common::Failure failure;
         if (parseResponse(failure, data)) {
            logger_->warn("[TrezorDevice::handleMessage] last message failure: {}"
               , getJSONReadableMessage(failure));
         }
         sendTxMessage(failure.message());
         reset();

         switch (failure.code()) {
            case common::Failure_FailureType_Failure_ActionCancelled:
               cancelledOnDevice();
               break;
            case common::Failure_FailureType_Failure_PinInvalid:
               invalidPin();
               break;
            default:
               operationFailed(failure.message());
               break;
         }
      }
      break;
   case MessageType_Features:
      {
         if (parseResponse(*features_, data)) {
            logger_->debug("[TrezorDevice] handleMessage Features, model: '{}' - {}.{}.{}"
               , features_->model(), features_->major_version(), features_->minor_version(), features_->patch_version());
            // + getJSONReadableMessage(features_)); 
         }
      }
   break;
   case MessageType_ButtonRequest:
      {
         common::ButtonRequest request;
         if (parseResponse(request, data)) {
            logger_->debug("[TrezorDevice] handleMessage ButtonRequest "
             + getJSONReadableMessage(request));
         }
         common::ButtonAck response;
         makeCall(response);
         sendTxMessage(/*HWInfoStatus::kPressButton*/"press the button");
         txSignedByUser_ = true;
      }
      break;
   case MessageType_PinMatrixRequest:
      {
         common::PinMatrixRequest request;
         if (parseResponse(request, data)) {
            requestPinMatrix();
            sendTxMessage(/*HWInfoStatus::kRequestPin*/"enter pin");
         }
      }
      break;
   case MessageType_PassphraseRequest:
      {
         common::PassphraseRequest request;
         if (parseResponse(request, data)) {
            requestHWPass(hasCapability(management::Features_Capability_Capability_PassphraseEntry));
            sendTxMessage(/*HWInfoStatus::kRequestPassphrase*/"enter passphrase");
         }
      }
      break;
   case MessageType_PublicKey:
      {
         bitcoin::PublicKey publicKey;
         if (parseResponse(publicKey, data)) {
            logger_->debug("[TrezorDevice::handleMessage] PublicKey: {}"
               , getJSONReadableMessage(publicKey));
         }
         if (cb) {
            const auto& outData = std::make_shared<XPubOut>();
            outData->xpub = publicKey.xpub();
            cb(outData);
            return;
         }
      }
      break;
   case MessageType_Address:
      {
         bitcoin::Address address;
         parseResponse(address, data);
      }
      break;
   case MessageType_TxRequest:
      {
         handleTxRequest(data, cb);
         sendTxMessage(txSignedByUser_ ? /*HWInfoStatus::kReceiveSignedTx*/"signed TX"
            : /*HWInfoStatus::kTransaction*/"transaction");
      }
      return;
   default:
      logger_->info("[TrezorDevice::handleMessage] {} - Unhandled message type", data.type);
      break;
   }
   if (cb) {
      const auto& noData = std::make_shared<NoDataOut>();
      noData->msgType = static_cast<MessageType>(data.type);
      cb(noData);
   }
}

bool TrezorDevice::parseResponse(google::protobuf::Message &msg, const trezor::MessageData& data)
{
   bool ok = msg.ParseFromString(data.message);
   if (ok) {
      logger_->debug("[TrezorDevice::parseResponse] {} - successfully parsed "
         "response", data.type);
   }
   else {
      logger_->error("[TrezorDevice::parseResponse] {} - failed to parse response"
         , data.type);
   }
   return ok;
}

void TrezorDevice::reset()
{
   currentTxSignReq_.reset();
   awaitingSignedTX_.clear();
   awaitingWalletInfo_ = {};
}

void TrezorDevice::handleTxRequest(const trezor::MessageData& data
   , const bs::WorkerPool::callback& cb)
{
   assert(currentTxSignReq_);
   bitcoin::TxRequest txRequest;
   if (parseResponse(txRequest, data)) {
      logger_->debug("[TrezorDevice] handleMessage TxRequest "
         + getJSONReadableMessage(txRequest));
   }

   if (txRequest.has_serialized() && txRequest.serialized().has_serialized_tx()) {
      awaitingSignedTX_ += txRequest.serialized().serialized_tx();
   }

   bitcoin::TxAck txAck;
   switch (txRequest.request_type())
   {
   case bitcoin::TxRequest_RequestType_TXINPUT:
   {
      if (!txRequest.details().tx_hash().empty()) {
         auto input = txAck.mutable_tx()->add_inputs();

         auto tx = prevTx(txRequest);
         if (tx.isInitialized()) {
            auto txIn = tx.getTxInCopy(txRequest.details().request_index());
            input->set_prev_hash(txIn.getOutPoint().getTxHash().copySwapEndian().toBinStr());
            input->set_prev_index(txIn.getOutPoint().getTxOutIndex());
            input->set_sequence(txIn.getSequence());
            input->set_script_sig(txIn.getScript().toBinStr());
         }

         logger_->debug("[TrezorDevice] handleTxRequest TXINPUT for prev hash"
             + getJSONReadableMessage(txAck));

         makeCall(txAck);
         break;
      }

      auto *type = new bitcoin::TxAck_TransactionType();
      bitcoin::TxAck_TransactionType_TxInputType *input = type->add_inputs();

      const int index = txRequest.details().request_index();
      assert(index >= 0 && index < currentTxSignReq_->armorySigner_.getTxInCount());
      auto spender = currentTxSignReq_->armorySigner_.getSpender(index);
      auto utxo = spender->getUtxo();

      auto address = bs::Address::fromUTXO(utxo);
      const auto purp = bs::hd::purpose(address.getType());

      auto bip32Paths = spender->getBip32Paths();
      if (bip32Paths.size() != 1) {
         throw std::logic_error("unexpected pubkey count for spender");
      }
      const auto& path = bip32Paths.begin()->second.getDerivationPathFromSeed();

      for (unsigned i=0; i<path.size(); i++) {
         //skip first index, it's the wallet root fingerprint
         input->add_address_n(path[i]);
      }

      input->set_prev_hash(utxo.getTxHash().copySwapEndian().toBinStr());
      input->set_prev_index(utxo.getTxOutIndex());

      switch (purp) {
         case bs::hd::Purpose::Native:
            input->set_script_type(bitcoin::SPENDWITNESS);
            input->set_amount(utxo.getValue());
            break;
         case bs::hd::Purpose::Nested:
            input->set_script_type(bitcoin::SPENDP2SHWITNESS);
            input->set_amount(utxo.getValue());
            break;
         case bs::hd::Purpose::NonSegWit:
            input->set_script_type(bitcoin::SPENDADDRESS);
            // No need to set amount for legacy input, will be computes from prev tx
            break;
      }

      if (currentTxSignReq_->RBF) {
         input->set_sequence(UINT32_MAX - 2);
      }

      txAck.set_allocated_tx(type);

      logger_->debug("[TrezorDevice] handleTxRequest TXINPUT"
          + getJSONReadableMessage(txAck));

      makeCall(txAck);
   }
   break;
   case bitcoin::TxRequest_RequestType_TXOUTPUT:
   {
      // Legacy inputs support
      if (!txRequest.details().tx_hash().empty()) {
         auto tx = prevTx(txRequest);
         auto binOutput = txAck.mutable_tx()->add_bin_outputs();
         if (tx.isInitialized()) {
            auto txOut = tx.getTxOutCopy(txRequest.details().request_index());
            binOutput->set_amount(txOut.getValue());
            binOutput->set_script_pubkey(txOut.getScript().toBinStr());
         }

         logger_->debug("[TrezorDevice] handleTxRequest TXOUTPUT for prev hash"
             + getJSONReadableMessage(txAck));

         makeCall(txAck);
         break;
      }

      auto *type = new bitcoin::TxAck_TransactionType();
      bitcoin::TxAck_TransactionType_TxOutputType *output = type->add_outputs();

      const auto index = txRequest.details().request_index();
      auto bsOutput = currentTxSignReq_->armorySigner_.getRecipient(index);
      auto address = bs::Address::fromRecipient(bsOutput);

      if (currentTxSignReq_->change.address != address) { // general output
         output->set_address(address.display());
         output->set_amount(bsOutput->getValue());
         //output->set_script_type(bitcoin::TxAck_TransactionType_TxOutputType_OutputScriptType_PAYTOADDRESS);
         output->set_script_type(bitcoin::PAYTOADDRESS);
      } else {
         const auto &change = currentTxSignReq_->change;
         output->set_amount(change.value);

         const auto purp = bs::hd::purpose(change.address.getType());

         if (change.index.empty()) {
            throw std::logic_error(fmt::format("can't find change address index for '{}'", change.address.display()));
         }

         auto path = getDerivationPath(testNet_, purp);
         path.append(bs::hd::Path::fromString(change.index));
         for (const uint32_t add : path) {
            output->add_address_n(add);
         }

         const auto changeType = change.address.getType();
         bitcoin::OutputScriptType scriptType;
         if (changeType == AddressEntryType_P2SH) {
            scriptType = bitcoin::PAYTOP2SHWITNESS;
         }
         else if (changeType == AddressEntryType_P2WPKH) {
            scriptType = bitcoin::PAYTOWITNESS;
         }
         else if (changeType == AddressEntryType_P2PKH) {
            scriptType = bitcoin::PAYTOADDRESS;
         } else {
            throw std::runtime_error(fmt::format("unexpected changeType: {}", static_cast<int>(changeType)));
         }

         output->set_script_type(scriptType);
      }

      txAck.set_allocated_tx(type);
      logger_->debug("[TrezorDevice] handleTxRequest TXOUTPUT"
         + getJSONReadableMessage(txAck));

      makeCall(txAck);
   }
   break;
   case bitcoin::TxRequest_RequestType_TXMETA:
   {
      // Return previous tx details for legacy inputs
      // See https://wiki.trezor.io/Developers_guide:Message_Workflows
      auto tx = prevTx(txRequest);
      auto data = txAck.mutable_tx();
      if (tx.isInitialized()) {
         data->set_version(tx.getVersion());
         data->set_lock_time(tx.getLockTime());
         data->set_inputs_cnt(tx.getNumTxIn());
         data->set_outputs_cnt(tx.getNumTxOut());
      }

      logger_->debug("[TrezorDevice] handleTxRequest TXMETA"
         + getJSONReadableMessage(txAck));

      makeCall(txAck);
   }
   break;
   case bitcoin::TxRequest_RequestType_TXFINISHED:
   {
      if (cb) {
         const auto& txOut = std::make_shared<TXOut>();
         txOut->signedTX = awaitingSignedTX_;
         cb(txOut);
      }
      sendTxMessage(/*HWInfoStatus::kTransactionFinished*/"TX finished");
      reset();
   }
   break;
   default:
      break;
   }
}

void TrezorDevice::sendTxMessage(const std::string &status)
{
   if (!currentTxSignReq_) {
      return;
   }
   //emit deviceTxStatusChanged(status);
}

Tx TrezorDevice::prevTx(const bitcoin::TxRequest &txRequest)
{
   auto txHash = BinaryData::fromString(txRequest.details().tx_hash()).swapEndian();
   try
   {
      return currentTxSignReq_->armorySigner_.getSupportingTx(txHash);
   }
   catch (const std::exception&)
   {
      SPDLOG_LOGGER_ERROR(logger_, "can't find prev TX {}", txHash.toHexStr(1));
      return {};
   }
}

bool TrezorDevice::hasCapability(const management::Features_Capability& cap) const
{
   return std::find(features_->capabilities().begin(), features_->capabilities().end(), cap)
         != features_->capabilities().end();
}

bool TrezorDevice::isFirmwareSupported() const
{
   auto verIt = kMinVersion.find(features_->model());
   if (verIt == kMinVersion.end()) {
      return false;
   }

   const auto &minVer = verIt->second;
   if (features_->major_version() > minVer[0]) {
      return true;
   }
   if (features_->major_version() < minVer[0]) {
      return false;
   }
   if (features_->minor_version() > minVer[1]) {
      return true;
   }
   if (features_->minor_version() < minVer[1]) {
      return false;
   }
   return features_->patch_version() >= minVer[2];
}

std::string TrezorDevice::firmwareSupportedVersion() const
{
   auto verIt = kMinVersion.find(features_->model());
   if (verIt == kMinVersion.end()) {
      return fmt::format("Unknown model: {}", features_->model());
   }
   const auto &minVer = verIt->second;
   return fmt::format("Please update wallet firmware to version {}.{}.{} or later"
      , minVer[0], minVer[1], minVer[2]);
}
