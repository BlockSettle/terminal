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
      stream.writeRawData(QByteArray::fromRawData(
         serialized_msg.c_str(), serialized_msg.size() * sizeof(uint32_t)).toHex()
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
}

TrezorDevice::TrezorDevice(const std::shared_ptr<ConnectionManager> &connectionManager
   , const QPointer<TrezorClient> &client, QObject* parent)
   : QObject(parent)
   , connectionManager_(connectionManager)
   , client_(std::move(client))
{
}

TrezorDevice::~TrezorDevice()
{
}

DeviceKey TrezorDevice::deviceKey() const
{
   return {
      QString::fromStdString(features_.label())
      , QString::fromStdString(features_.device_id())
      , QString::fromStdString(features_.vendor())
   };
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
   connectionManager_->GetLogger()->debug("[TrezorDevice] init - start retrieving public key from device " + features_.label());
   bitcoin::GetPublicKey message;
   // #TREZOR_INTEGRATION: shouldn't be hardcoded m/ 49'/ 1' / 0'
   message.add_address_n(static_cast<uint32_t>(0x80000031));
   message.add_address_n(static_cast<uint32_t>(0x80000001));
   message.add_address_n(static_cast<uint32_t>(0x80000000));
   message.set_coin_name("Testnet");

   if (cb) {
      setDataCallback(MessageType_PublicKey, std::move(cb));
   }

   makeCall(message);
}

void TrezorDevice::setMatrixPin(const std::string& pin)
{
   connectionManager_->GetLogger()->debug("[TrezorDevice] setMatrixPin - send matrix pin response");
   common::PinMatrixAck message;
   message.set_pin(pin);
   makeCall(message);
}

void TrezorDevice::cancel()
{
   connectionManager_->GetLogger()->debug("[TrezorDevice] cancel previous operation");
   management::Cancel message;
   makeCall(message);
}

void TrezorDevice::makeCall(const google::protobuf::Message &msg)
{
   client_->call(packMessage(msg), [this](QByteArray&& answer) {
      MessageData data = unpackMessage(answer);
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
         parseResponse(failure, data);
         resetCallbacks();
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
         parseResponse(request, data);
      }
      break;
   case MessageType_PinMatrixRequest:
      {
         common::PinMatrixRequest request;
         if (parseResponse(request, data)) {
            emit requestPinMatrix();
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

void TrezorDevice::resetCallbacks()
{
   awaitingCallbackNoData_.clear();
   awaitingCallbackData_.clear();
}

void TrezorDevice::setCallbackNoData(MessageType type, AsyncCallBack&& cb)
{
   awaitingCallbackNoData_[type] = std::move(cb);
}

void TrezorDevice::callbackNoData(MessageType type)
{
   auto iAwaiting = awaitingCallbackNoData_.find(type);
   if (iAwaiting != awaitingCallbackNoData_.end()) {
      iAwaiting->second();
      awaitingCallbackNoData_.erase(iAwaiting);
   }
}

void TrezorDevice::setDataCallback(MessageType type, AsyncCallBackCall&& cb)
{
   awaitingCallbackData_[type] = std::move(cb);
}

void TrezorDevice::dataCallback(MessageType type, QByteArray&& response)
{
   auto iAwaiting = awaitingCallbackData_.find(type);
   if (iAwaiting != awaitingCallbackData_.end()) {
      iAwaiting->second(std::move(response));
      awaitingCallbackData_.erase(iAwaiting);
   }
}
