/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "ZMQ_BIP15X_Msg.h"

void ZmqBipMsgBuilder::construct(const uint8_t *data, int dataSize, uint8_t type)
{
   //is this payload carrying a msgid?
   const bool insertMsgId = (type == ZMQ_MSGTYPE_SINGLEPACKET);

   BinaryWriter writer;

   writer.put_uint32_t(0);
   size_t headerSize = writer.getSize();

   writer.put_uint8_t(type);

   if (insertMsgId) {
      // we don't use msg ID for now but keep it for possible feature usage
      writer.put_uint32_t(0);
   }

   writer.put_BinaryData(data, uint32_t(dataSize));

   packet_ = writer.getData();
   uint32_t packetSize = uint32_t(packet_.getSize() - headerSize);
   std::memcpy(packet_.getPtr(), &packetSize, sizeof(packetSize));
}

ZmqBipMsgBuilder::ZmqBipMsgBuilder(const uint8_t *data, int dataSize, uint8_t type)
{
   construct(data, dataSize, type);
}

ZmqBipMsgBuilder::ZmqBipMsgBuilder(const std::vector<uint8_t> &data, uint8_t type)
{
   construct(data.data(), int(data.size()), type);
}

ZmqBipMsgBuilder::ZmqBipMsgBuilder(const BinaryDataRef &data, uint8_t type)
{
   construct(data.getPtr(), int(data.getSize()), type);
}

ZmqBipMsgBuilder::ZmqBipMsgBuilder(const std::string &data, uint8_t type)
{
   construct(reinterpret_cast<const uint8_t*>(data.data()), int(data.size()), type);
}

ZmqBipMsgBuilder::ZmqBipMsgBuilder(uint8_t type)
{
   construct(nullptr, 0, type);
}

ZmqBipMsgBuilder &ZmqBipMsgBuilder::encryptIfNeeded(BIP151Connection *conn)
{
   if (!conn) {
      return *this;
   }

   size_t plainTextLen = packet_.getSize();
   size_t cipherTextLen = plainTextLen + POLY1305MACLEN;
   BinaryData packetEnc(cipherTextLen);

   int rc = conn->assemblePacket(packet_.getPtr(), plainTextLen, packetEnc.getPtr(), cipherTextLen);
   if (rc != 0) {
      //failed to encrypt, abort
      throw std::runtime_error("failed to encrypt packet, aborting");
   }
   packet_ = std::move(packetEnc);

   return *this;
}

BinaryData ZmqBipMsgBuilder::build()
{
   return std::move(packet_);
}

ZmqBipMsg ZmqBipMsg::parsePacket(const BinaryDataRef &packet)
{
   try {
      BinaryRefReader reader(packet);

      uint32_t packetLen = reader.get_uint32_t();
      if (packetLen != reader.getSizeRemaining()) {
         return {};
      }

      uint8_t type = reader.get_uint8_t();

      switch (type)
      {
      case ZMQ_MSGTYPE_SINGLEPACKET:
         // skip not used message ID
         reader.get_uint32_t();
         break;

      case ZMQ_MSGTYPE_AEAD_SETUP:
      case ZMQ_MSGTYPE_AEAD_PRESENT_PUBKEY:
      case ZMQ_MSGTYPE_AEAD_ENCINIT:
      case ZMQ_MSGTYPE_AEAD_ENCACK:
      case ZMQ_MSGTYPE_AEAD_REKEY:
      case ZMQ_MSGTYPE_AUTH_CHALLENGE:
      case ZMQ_MSGTYPE_AUTH_REPLY:
      case ZMQ_MSGTYPE_AUTH_PROPOSE:
      case ZMQ_MSGTYPE_HEARTBEAT:
      case ZMQ_MSGTYPE_DISCONNECT:
         break;

      default:
         return {};
      }

      ZmqBipMsg result;
      result.type_ = type;
      result.data_ = reader.get_BinaryDataRef(uint32_t(reader.getSizeRemaining()));
      return result;
   } catch (...) {
      return {};
   }
}
