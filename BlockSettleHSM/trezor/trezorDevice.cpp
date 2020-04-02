/*

***********************************************************************************
* Copyright (C) 2016 - , BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "trezorDevice.h"
#include "trezorClient.h"
#include "ConnectionManager.h"
#include "headless.pb.h"
#include "ProtobufHeadlessUtils.h"
#include "CoreWallet.h"
#include "Wallets/SyncWalletsManager.h"
#include "Wallets/SyncHDWallet.h"

#include <QDataStream>


// Protobuf
#include <google/protobuf/util/json_util.h>

using namespace hw::trezor::messages;

namespace {
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

   std::vector<uint32_t> getDerivationPath(bool testNet, bool isNestedSegwit)
   {
      std::vector<uint32_t> path;
      if (isNestedSegwit) {
         path.push_back(0x80000031);
      }
      else {
         path.push_back(0x80000054);
      }

      if (testNet) {
         path.push_back(0x80000001);
      }
      else {
         path.push_back(0x80000000);
      }

      path.push_back(0x80000000);

      return path;
   }

   const std::string tesNetCoin = "Testnet";
   // Messages to show in UI
   const QString requestPin = QObject::tr("Please enter the pin from device");
   const QString pressButton = QObject::tr("Please press button on Trezor device");
   const QString transaction = QObject::tr("Setup transaction...");
   const QString transactionFinished = QObject::tr("Transaction signing finished with success");
   const QString canceledByUser = QObject::tr("Canceled by user");

}

TrezorDevice::TrezorDevice(const std::shared_ptr<ConnectionManager> &connectionManager, std::shared_ptr<bs::sync::WalletsManager> walletManager
   , bool testNet, const QPointer<TrezorClient> &client, QObject* parent)
   : HSMDeviceAbstract(parent)
   , connectionManager_(connectionManager)
   , walletManager_(walletManager)
   , client_(std::move(client))
   , testNet_(testNet)
{
}

TrezorDevice::~TrezorDevice()
{
}

DeviceKey TrezorDevice::key() const
{
   return {
      QString::fromStdString(features_.label())
      , QString::fromStdString(features_.device_id())
      , QString::fromStdString(features_.vendor())
      , {}
      , {}
      , type()
   };
}

DeviceType TrezorDevice::type() const
{
   return DeviceType::HWTrezor;
}

void TrezorDevice::init(AsyncCallBack&& cb)
{
   connectionManager_->GetLogger()->debug("[TrezorDevice] init - start init call ");
   management::Initialize message;
   message.set_session_id(client_->getSessionId());

   if (cb) {
      setCallbackNoData(MessageType_Features, std::move(cb));
   }

   makeCall(message);
}

void TrezorDevice::getPublicKey(AsyncCallBackCall&& cb)
{
   awaitingWalletInfo_ = {};

   // We cannot get all data from one call so we make three calls:
   // fetching first address for "m/0'" as wallet id
   // fetching first address for "m/49'" as nested segwit xpub
   // fetching first address for "m/84'" as native segwit xpub

   AsyncCallBackCall cbFinal = [this, originCb = std::move(cb)](QVariant &&data) mutable {
      awaitingWalletInfo_.info_.xpubNativeSegwit_ = data.toByteArray().toStdString();
      
      // General data
      awaitingWalletInfo_.info_.label_ = features_.label();
      awaitingWalletInfo_.info_.deviceId_ = features_.device_id();
      awaitingWalletInfo_.info_.vendor_ = features_.vendor();

      originCb(QVariant::fromValue<>(awaitingWalletInfo_));
   };

   // Fetching native segwit
   AsyncCallBackCall cbNative = [this, originCbFinal = std::move(cbFinal)](QVariant &&data) mutable {
      awaitingWalletInfo_.info_.xpubNestedSegwit_ = data.toByteArray().toStdString();

      connectionManager_->GetLogger()->debug("[TrezorDevice] init - start retrieving native segwit public key from device "
         + features_.label());
      bitcoin::GetPublicKey message;
      for (const uint32_t add : getDerivationPath(testNet_, false)) {
         message.add_address_n(add);
      }
      if (testNet_) {
         message.set_coin_name(tesNetCoin);
      }

      setDataCallback(MessageType_PublicKey, std::move(originCbFinal));
      makeCall(message);
   };

   // Fetching nested segwit
   AsyncCallBackCall cbNested = [this, originCbNative = std::move(cbNative)](QVariant &&data) mutable {
      awaitingWalletInfo_.info_.xpubRoot_ = data.toByteArray().toStdString();

      connectionManager_->GetLogger()->debug("[TrezorDevice] init - start retrieving nested segwit public key from device "
         + features_.label());
      bitcoin::GetPublicKey message;
      for (const uint32_t add : getDerivationPath(testNet_, true)) {
         message.add_address_n(add);
      }
      if (testNet_) {
         message.set_coin_name(tesNetCoin);
      }

      setDataCallback(MessageType_PublicKey, std::move(originCbNative));
      makeCall(message);
   };

   // Fetching walletId
   connectionManager_->GetLogger()->debug("[TrezorDevice] init - start retrieving root public key from device "
      + features_.label());
   bitcoin::GetPublicKey message;
   message.add_address_n(0x80000000);
   if (testNet_) {
      message.set_coin_name(tesNetCoin);
   }

   // Fetching nested segwit
   setDataCallback(MessageType_PublicKey, std::move(cbNested));
   makeCall(message);
}

void TrezorDevice::setMatrixPin(const std::string& pin)
{
   connectionManager_->GetLogger()->debug("[TrezorDevice] setMatrixPin - send matrix pin response");
   common::PinMatrixAck message;
   message.set_pin(pin);
   makeCall(message);
}

void TrezorDevice::setPassword(const std::string& password)
{
   // #TREZOR_INTEGRATION: DO NOT FORGET PASSWORD CASE
}

void TrezorDevice::cancel()
{
   connectionManager_->GetLogger()->debug("[TrezorDevice] cancel previous operation");
   management::Cancel message;
   makeCall(message);
   sendTxMessage(canceledByUser);
}

void TrezorDevice::signTX(const QVariant& reqTX, AsyncCallBackCall&& cb /*= nullptr*/)
{
   Blocksettle::Communication::headless::SignTxRequest request;
   bool res = request.ParseFromString(reqTX.toByteArray().toStdString());
   if (!res) {
      connectionManager_->GetLogger()->debug("[TrezorDevice] signTX - failed to parse transaction request ");
      return;
   }

   currentTxSignReq_.reset(new bs::core::wallet::TXSignRequest(bs::signer::pbTxRequestToCore(request)));
   connectionManager_->GetLogger()->debug("[TrezorDevice] SignTX - specify init data to " + features_.label());

   const int change = static_cast<bool>(currentTxSignReq_->change.value) ? 1 : 0;

   bitcoin::SignTx message;
   message.set_inputs_count(currentTxSignReq_->inputs.size());
   message.set_outputs_count(currentTxSignReq_->recipients.size() + change);
   if (testNet_) {
      message.set_coin_name(tesNetCoin);
   }

   if (cb) {
      setDataCallback(MessageType_TxRequest, std::move(cb));
   }

   awaitingTransaction_ = {};
   makeCall(message);
}

void TrezorDevice::makeCall(const google::protobuf::Message &msg)
{
   client_->call(packMessage(msg), [this](QVariant&& answer) {
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
            connectionManager_->GetLogger()->debug("[TrezorDevice] handleMessage last message failure "
             + getJSONReadableMessage(failure));
         }
         sendTxMessage(QString::fromStdString(failure.message()));
         resetCaches();
         emit operationFailed();
      }
      break;
   case MessageType_Features:
      {
         if (parseResponse(features_, data)) {
            connectionManager_->GetLogger()->debug("[TrezorDevice] handleMessage Features ");
            // + getJSONReadableMessage(features_)); 
         }
      }
   break;
   case MessageType_ButtonRequest:
      {
         common::ButtonRequest request;
         if (parseResponse(request, data)) {
            connectionManager_->GetLogger()->debug("[TrezorDevice] handleMessage ButtonRequest "
             + getJSONReadableMessage(request));
         }
         common::ButtonAck response;
         makeCall(response);
         sendTxMessage(pressButton);
      }
      break;
   case MessageType_PinMatrixRequest:
      {
         common::PinMatrixRequest request;
         if (parseResponse(request, data)) {
            emit requestPinMatrix();
            sendTxMessage(requestPin);
         }
      }
      break;
   case MessageType_PublicKey:
      {
         bitcoin::PublicKey publicKey;
         if (parseResponse(publicKey, data)) {
            connectionManager_->GetLogger()->debug("[TrezorDevice] handleMessage PublicKey" //);
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
         sendTxMessage(transaction);
      }
      break;
   default:
      {
         connectionManager_->GetLogger()->debug("[TrezorDevice] handleMessage " + std::to_string(data.msg_type_) + " - Unhandled message type");
      }
      break;
   }

   callbackNoData(static_cast<MessageType>(data.msg_type_));
}

bool TrezorDevice::parseResponse(google::protobuf::Message &msg, const MessageData& data)
{
   bool ok = msg.ParseFromString(data.message_);
   if (ok) {
      connectionManager_->GetLogger()->debug("[TrezorDevice] handleMessage " +
         std::to_string(data.msg_type_) + " - successfully parsed response");
   }
   else {
      connectionManager_->GetLogger()->debug("[TrezorDevice] handleMessage " +
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
      connectionManager_->GetLogger()->debug("[TrezorDevice] handleMessage TxRequest "
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
      auto *type = new bitcoin::TxAck_TransactionType();
      bitcoin::TxAck_TransactionType_TxInputType *input = type->add_inputs();

      const int index = txRequest.details().request_index();
      assert(index >= 0 && index < currentTxSignReq_->inputs.size());
      auto utxo = currentTxSignReq_->inputs[index];

      auto address = bs::Address::fromUTXO(utxo);
      bool isNestedSegwit = (address.getType() == AddressEntryType_P2SH);

      std::string addrIndex;
      for (const auto &walletId : currentTxSignReq_->walletIds) {
         auto wallet = walletManager_->getWalletById(walletId);
         addrIndex = wallet->getAddressIndex(address);
         if (!addrIndex.empty()) {
            break;
         }
      }
      if (addrIndex.empty()) {
         throw std::logic_error(fmt::format("can't find input address index for '{}'", address.display()));
      }

      for (const uint32_t add : getDerivationPath(testNet_, isNestedSegwit)) {
         input->add_address_n(add);
      }
      const auto path = bs::hd::Path::fromString(addrIndex);
      for (int i = 0; i < path.length(); ++i) {
         input->add_address_n(path.get(i));
      }

      input->set_prev_hash(utxo.getTxHash().copySwapEndian().toBinStr());
      input->set_prev_index(utxo.getTxOutIndex());
      input->set_script_type(isNestedSegwit ? bitcoin::SPENDP2SHWITNESS : bitcoin::SPENDWITNESS);
      input->set_amount(utxo.getValue());

      txAck.set_allocated_tx(type);

      connectionManager_->GetLogger()->debug("[TrezorDevice] handleTxRequest TXINPUT"
          + getJSONReadableMessage(txAck));

      makeCall(txAck);
   }
   break;
   case bitcoin::TxRequest_RequestType_TXOUTPUT:
   {
      auto *type = new bitcoin::TxAck_TransactionType();
      bitcoin::TxAck_TransactionType_TxOutputType *output = type->add_outputs();

      const int index = txRequest.details().request_index();

      if (currentTxSignReq_->recipients.size() > index) { // general output
         auto &bsOutput = currentTxSignReq_->recipients[index];

         auto address = bs::Address::fromRecipient(bsOutput);
         output->set_address(address.display());
         output->set_amount(bsOutput->getValue());
         output->set_script_type(bitcoin::TxAck_TransactionType_TxOutputType_OutputScriptType_PAYTOADDRESS);
      }
      else if (currentTxSignReq_->recipients.size() == index && currentTxSignReq_->change.value > 0) { // one last for change
         const auto &change = currentTxSignReq_->change;
         output->set_amount(change.value);

         bool isNestedSegwit = (change.address.getType() == AddressEntryType_P2SH);

         if (change.index.empty()) {
            throw std::logic_error(fmt::format("can't find change address index for '{}'", change.address.display()));
         }
         for (const uint32_t add : getDerivationPath(testNet_, isNestedSegwit)) {
            output->add_address_n(add);
         }
         const auto path = bs::hd::Path::fromString(change.index);
         for (int i = 0; i < path.length(); ++i) {
            output->add_address_n(path.get(i));
         }

         output->set_script_type(isNestedSegwit ? bitcoin::TxAck_TransactionType_TxOutputType_OutputScriptType_PAYTOP2SHWITNESS
                                                : bitcoin::TxAck_TransactionType_TxOutputType_OutputScriptType_PAYTOWITNESS);
      }
      else {
         // Shouldn't be here
         assert(false);
      }

      txAck.set_allocated_tx(type);
      connectionManager_->GetLogger()->debug("[TrezorDevice] handleTxRequest TXOUTPUT"
         + getJSONReadableMessage(txAck));

      makeCall(txAck);
   }
   break;
   case bitcoin::TxRequest_RequestType_TXFINISHED:
   {
      dataCallback(MessageType_TxRequest, QVariant::fromValue<>(awaitingTransaction_));
      sendTxMessage(transactionFinished);
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
