/*

***********************************************************************************
* Copyright (C) 2020 - 2021, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "trezorDevice.h"
#include "trezorClient.h"
#include "ConnectionManager.h"
#include "headless.pb.h"
#include "Wallets/ProtobufHeadlessUtils.h"
#include "CoreWallet.h"
#include "Wallets/SyncWalletsManager.h"
#include "Wallets/SyncHDWallet.h"

#include <QDataStream>


// Protobuf
#include <google/protobuf/util/json_util.h>

using namespace hw::trezor::messages;

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

   MessageData unpackMessage(const QByteArray& response)
   {
      QDataStream stream(response);
      MessageData ret;

      ret.msg_type_ = QByteArray::fromHex(response.mid(0, 4).toHex()).toInt(nullptr, 16);
      ret.length_ = QByteArray::fromHex(response.mid(4, 8).toHex()).toInt(nullptr, 16);
      ret.message_ = QByteArray::fromHex(response.mid(12, 2 * ret.length_)).toStdString();

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

   const std::string tesNetCoin = "Testnet";
}

TrezorDevice::TrezorDevice(const std::shared_ptr<spdlog::logger> &logger
   , std::shared_ptr<bs::sync::WalletsManager> walletManager
   , bool testNet, const QPointer<TrezorClient> &client, QObject* parent)
   : HwDeviceInterface(parent)
   , logger_(logger), walletManager_(walletManager)
   , client_(std::move(client))
   , testNet_(testNet)
{}

TrezorDevice::~TrezorDevice() = default;

DeviceKey TrezorDevice::key() const
{
   QString walletId;
   QString status;
   if (!xpubRoot_.empty()) {
      auto expectedWalletId = bs::core::wallet::computeID(
         BinaryData::fromString(xpubRoot_)).toBinStr();

      auto importedWallets = walletManager_->getHwWallets(
         bs::wallet::HardwareEncKey::WalletType::Trezor, features_.device_id());

      for (const auto &imported : importedWallets) {
         if (expectedWalletId == imported) {
            walletId = QString::fromStdString(expectedWalletId);
            break;
         }
      }
   }
   else {
      status = tr("Not initialized");
   }

   return {
      QString::fromStdString(features_.label())
      , QString::fromStdString(features_.device_id())
      , QString::fromStdString(features_.vendor())
      , walletId
      , status
      , type()
   };
}

DeviceType TrezorDevice::type() const
{
   return DeviceType::HWTrezor;
}

void TrezorDevice::init(AsyncCallBack&& cb)
{
   logger_->debug("[TrezorDevice] init - start init call");
   management::Initialize message;
   message.set_session_id(client_->getSessionId());

   setCallbackNoData(MessageType_Features, std::move(cb));
   makeCall(message);
}

void TrezorDevice::getPublicKey(AsyncCallBackCall&& cb)
{
   awaitingWalletInfo_ = {};
   // General data
   awaitingWalletInfo_.info_.type = bs::wallet::HardwareEncKey::WalletType::Trezor;
   awaitingWalletInfo_.info_.label = features_.label();
   awaitingWalletInfo_.info_.deviceId = features_.device_id();
   awaitingWalletInfo_.info_.vendor = features_.vendor();
   awaitingWalletInfo_.info_.xpubRoot = xpubRoot_;

   awaitingWalletInfo_.isFirmwareSupported_ = isFirmwareSupported();
   if (!awaitingWalletInfo_.isFirmwareSupported_) {
      awaitingWalletInfo_.firmwareSupportedMsg_ = firmwareSupportedVersion();
      cb(QVariant::fromValue<>(awaitingWalletInfo_));
      return;
   }

   // We cannot get all data from one call so we make four calls:
   // fetching first address for "m/0'" as wallet id
   // fetching first address for "m/84'" as native segwit xpub
   // fetching first address for "m/49'" as nested segwit xpub
   // fetching first address for "m/44'" as legacy xpub

   AsyncCallBackCall cbLegacy = [this, cb = std::move(cb)](QVariant &&data) mutable {
      awaitingWalletInfo_.info_.xpubLegacy = data.toByteArray().toStdString();
      
      cb(QVariant::fromValue<>(awaitingWalletInfo_));
   };

   AsyncCallBackCall cbNested = [this, cbLegacy = std::move(cbLegacy)](QVariant &&data) mutable {
      awaitingWalletInfo_.info_.xpubNestedSegwit = data.toByteArray().toStdString();

      logger_->debug("[TrezorDevice] init - start retrieving legacy public key from device {}"
         , features_.label());
      bitcoin::GetPublicKey message;
      for (const uint32_t add : getDerivationPath(testNet_, bs::hd::Purpose::NonSegWit)) {
         message.add_address_n(add);
      }
      if (testNet_) {
         message.set_coin_name(tesNetCoin);
      }

      setDataCallback(MessageType_PublicKey, std::move(cbLegacy));
      makeCall(message);
   };

   AsyncCallBackCall cbNative = [this, cbNested = std::move(cbNested)](QVariant &&data) mutable {
      awaitingWalletInfo_.info_.xpubNativeSegwit = data.toByteArray().toStdString();

      logger_->debug("[TrezorDevice] init - start retrieving nested segwit public key from device {}"
         , features_.label());
      bitcoin::GetPublicKey message;
      for (const uint32_t add : getDerivationPath(testNet_, bs::hd::Purpose::Nested)) {
         message.add_address_n(add);
      }

      if (testNet_) {
         message.set_coin_name(tesNetCoin);
      }

      setDataCallback(MessageType_PublicKey, std::move(cbNested));
      makeCall(message);
   };

   logger_->debug("[TrezorDevice] init - start retrieving native segwit public key from device {}"
      , features_.label());
   bitcoin::GetPublicKey message;
   for (const uint32_t add : getDerivationPath(testNet_, bs::hd::Purpose::Native)) {
      message.add_address_n(add);
   }

   if (testNet_) {
      message.set_coin_name(tesNetCoin);
   }

   setDataCallback(MessageType_PublicKey, std::move(cbNative));
   makeCall(message);
}

void TrezorDevice::setMatrixPin(const std::string& pin)
{
   logger_->debug("[TrezorDevice] setMatrixPin - send matrix pin response");
   common::PinMatrixAck message;
   message.set_pin(pin);
   makeCall(message);
}

void TrezorDevice::setPassword(const std::string& password, bool enterOnDevice)
{
   logger_->debug("[TrezorDevice] setPassword - send passphrase response");
   common::PassphraseAck message;
   if (enterOnDevice) {
      message.set_on_device(true);
   } else {
      message.set_passphrase(password);
   }
   makeCall(message);
}

void TrezorDevice::cancel()
{
   logger_->debug("[TrezorDevice] cancel previous operation");
   management::Cancel message;
   makeCall(message);
   sendTxMessage(HWInfoStatus::kCancelledByUser);
}

void TrezorDevice::clearSession(AsyncCallBack&& cb)
{
   logger_->debug("[TrezorDevice] cancel previous operation");
   management::/*ClearSession*/EndSession message;

   if (cb) {
      setCallbackNoData(MessageType_Success, std::move(cb));
   }

   makeCall(message);
}


void TrezorDevice::signTX(const bs::core::wallet::TXSignRequest &reqTX, AsyncCallBackCall&& cb /*= nullptr*/)
{
   currentTxSignReq_.reset(new bs::core::wallet::TXSignRequest(reqTX));
   logger_->debug("[TrezorDevice] SignTX - specify init data to " + features_.label());

   bitcoin::SignTx message;
   message.set_inputs_count(currentTxSignReq_->armorySigner_.getTxInCount());
   message.set_outputs_count(currentTxSignReq_->armorySigner_.getTxOutCount());
   if (testNet_) {
      message.set_coin_name(tesNetCoin);
   }

   if (cb) {
      setDataCallback(MessageType_TxRequest, std::move(cb));
   }

   awaitingTransaction_ = {};
   makeCall(message);
}

void TrezorDevice::retrieveXPubRoot(AsyncCallBack&& cb)
{
   // Fetching walletId
   logger_->debug("[TrezorDevice] init - start retrieving root public key from device "
      + features_.label());
   bitcoin::GetPublicKey message;
   message.add_address_n(bs::hd::hardFlag);
   if (testNet_) {
      message.set_coin_name(tesNetCoin);
   }

   auto saveXpubRoot = [caller = QPointer<TrezorDevice>(this), cb = std::move(cb)](QVariant&& data) {
      if (!caller) {
         return;
      }

      caller->xpubRoot_ = data.toByteArray().toStdString();
      if (cb) {
         cb();
      }
   };

   setDataCallback(MessageType_PublicKey, std::move(saveXpubRoot));
   makeCall(message);
}

void TrezorDevice::makeCall(const google::protobuf::Message &msg)
{
   client_->call(packMessage(msg), [this](QVariant&& answer) {
      if (answer.isNull()) {
         emit operationFailed(QLatin1String("Network error"));
         resetCaches();
      }

      MessageData data = unpackMessage(answer.toByteArray());
      handleMessage(data);
   });
}

void TrezorDevice::handleMessage(const MessageData& data)
{
   switch (static_cast<MessageType>(data.msg_type_)) {
   case MessageType_Success:
      {
         common::Success success;
         parseResponse(success, data);
      }
      break;
   case MessageType_Failure:
      {
         common::Failure failure;
         if (parseResponse(failure, data)) {
            logger_->debug("[TrezorDevice] handleMessage last message failure "
             + getJSONReadableMessage(failure));
         }
         sendTxMessage(QString::fromStdString(failure.message()));
         resetCaches();

         switch (failure.code()) {
            case common::Failure_FailureType_Failure_ActionCancelled:
               emit cancelledOnDevice();
               break;
            case common::Failure_FailureType_Failure_PinInvalid:
               emit invalidPin();
               break;
            default:
               emit operationFailed(QString::fromStdString(failure.message()));
               break;
         }
      }
      break;
   case MessageType_Features:
      {
         if (parseResponse(features_, data)) {
            logger_->debug("[TrezorDevice] handleMessage Features, model: '{}' - {}.{}.{}"
               , features_.model(), features_.major_version(), features_.minor_version(), features_.patch_version());
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
         sendTxMessage(HWInfoStatus::kPressButton);
         txSignedByUser_ = true;
      }
      break;
   case MessageType_PinMatrixRequest:
      {
         common::PinMatrixRequest request;
         if (parseResponse(request, data)) {
            emit requestPinMatrix();
            sendTxMessage(HWInfoStatus::kRequestPin);
         }
      }
      break;
   case MessageType_PassphraseRequest:
      {
         common::PassphraseRequest request;
         if (parseResponse(request, data)) {
            emit requestHWPass(hasCapability(management::Features_Capability_Capability_PassphraseEntry));
            sendTxMessage(HWInfoStatus::kRequestPassphrase);
         }
      }
      break;
   case MessageType_PublicKey:
      {
         bitcoin::PublicKey publicKey;
         if (parseResponse(publicKey, data)) {
            logger_->debug("[TrezorDevice] handleMessage PublicKey" //);
             + getJSONReadableMessage(publicKey));
         }
         dataCallback(MessageType_PublicKey, QByteArray::fromStdString(publicKey.xpub()));
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
         handleTxRequest(data);
         sendTxMessage(txSignedByUser_ ? HWInfoStatus::kReceiveSignedTx : HWInfoStatus::kTransaction);
      }
      break;
   default:
      {
         logger_->debug("[TrezorDevice] handleMessage " + std::to_string(data.msg_type_) + " - Unhandled message type");
      }
      break;
   }

   callbackNoData(static_cast<MessageType>(data.msg_type_));
}

bool TrezorDevice::parseResponse(google::protobuf::Message &msg, const MessageData& data)
{
   bool ok = msg.ParseFromString(data.message_);
   if (ok) {
      logger_->debug("[TrezorDevice] handleMessage " +
         std::to_string(data.msg_type_) + " - successfully parsed response");
   }
   else {
      logger_->debug("[TrezorDevice] handleMessage " +
         std::to_string(data.msg_type_) + " - failed to parse response");
   }

   return ok;
}

void TrezorDevice::resetCaches()
{
   awaitingCallbackNoData_.clear();
   awaitingCallbackData_.clear();
   currentTxSignReq_.reset(nullptr);
   awaitingTransaction_ = {};
   awaitingWalletInfo_ = {};
}

void TrezorDevice::setCallbackNoData(MessageType type, AsyncCallBack&& cb)
{
   awaitingCallbackNoData_[type] = std::move(cb);
}

void TrezorDevice::callbackNoData(MessageType type)
{
   auto iAwaiting = awaitingCallbackNoData_.find(type);
   if (iAwaiting != awaitingCallbackNoData_.end()) {
      auto cb = std::move(iAwaiting->second);
      awaitingCallbackNoData_.erase(iAwaiting);
      cb();
   }
}

void TrezorDevice::setDataCallback(MessageType type, AsyncCallBackCall&& cb)
{
   awaitingCallbackData_[type] = std::move(cb);
}

void TrezorDevice::dataCallback(MessageType type, QVariant&& response)
{
   auto iAwaiting = awaitingCallbackData_.find(type);
   if (iAwaiting != awaitingCallbackData_.end()) {
      auto cb = std::move(iAwaiting->second);
      awaitingCallbackData_.erase(iAwaiting);
      cb(std::move(response));
   }
}

void TrezorDevice::handleTxRequest(const MessageData& data)
{
   assert(currentTxSignReq_);
   bitcoin::TxRequest txRequest;
   if (parseResponse(txRequest, data)) {
      logger_->debug("[TrezorDevice] handleMessage TxRequest "
         + getJSONReadableMessage(txRequest));
   }

   if (txRequest.has_serialized() && txRequest.serialized().has_serialized_tx()) {
      awaitingTransaction_.signedTx += txRequest.serialized().serialized_tx();
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
      dataCallback(MessageType_TxRequest, QVariant::fromValue<>(awaitingTransaction_));
      sendTxMessage(HWInfoStatus::kTransactionFinished);
      resetCaches();
   }
   break;
   default:
      break;
   }
}

void TrezorDevice::sendTxMessage(const QString& status)
{
   if (!currentTxSignReq_) {
      return;
   }

   emit deviceTxStatusChanged(status);
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

bool TrezorDevice::hasCapability(management::Features::Capability cap) const
{
   return std::find(features_.capabilities().begin(), features_.capabilities().end(), cap)
         != features_.capabilities().end();
}

bool TrezorDevice::isFirmwareSupported() const
{
   auto verIt = kMinVersion.find(features_.model());
   if (verIt == kMinVersion.end()) {
      return false;
   }

   const auto &minVer = verIt->second;
   if (features_.major_version() > minVer[0]) {
      return true;
   }
   if (features_.major_version() < minVer[0]) {
      return false;
   }
   if (features_.minor_version() > minVer[1]) {
      return true;
   }
   if (features_.minor_version() < minVer[1]) {
      return false;
   }
   return features_.patch_version() >= minVer[2];
}

std::string TrezorDevice::firmwareSupportedVersion() const
{
   auto verIt = kMinVersion.find(features_.model());
   if (verIt == kMinVersion.end()) {
      return fmt::format("Unknown model: {}", features_.model());
   }
   const auto &minVer = verIt->second;
   return fmt::format("Please update wallet firmware to version {}.{}.{} or later"
      , minVer[0], minVer[1], minVer[2]);
}
