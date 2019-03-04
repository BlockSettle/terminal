#include "ZMQ_BIP15X_Msg.h"

using namespace std;

void ZMQ_BIP15X_Msg::reset()
{
   packets_.clear();
   id_ = UINT32_MAX;
   type_ = UINT8_MAX;
   packetCount_ = UINT32_MAX;
}

///////////////////////////////////////////////////////////////////////////////
bool ZMQ_BIP15X_Msg::parsePacket(const BinaryDataRef& dataRef)
{
   if (dataRef.getSize() == 0)
      return false;

   BinaryRefReader brrPacket(dataRef);
   auto packetlen = brrPacket.get_uint32_t();
   if (packetlen != brrPacket.getSizeRemaining())
   {
      LOGERR << "invalid packet size";
      return false;
   }

   auto dataSlice = brrPacket.get_BinaryDataRef(packetlen);
   BinaryRefReader brrSlice(dataSlice);

   auto msgType = brrSlice.get_uint8_t();

   switch (msgType)
   {
   case ZMQ_MSGTYPE_SINGLEPACKET:
   {
      return parseSinglePacket(dataSlice);
   }

   case ZMQ_MSGTYPE_FRAGMENTEDPACKET_HEADER:
   {
      return parseFragmentedMessageHeader(dataSlice);
   }

   case ZMQ_MSGTYPE_FRAGMENTEDPACKET_FRAGMENT:
   {
      return parseMessageFragment(dataSlice);
   }

   case ZMQ_MSGTYPE_AEAD_SETUP:
   case ZMQ_MSGTYPE_AEAD_PRESENT_PUBKEY:
   case ZMQ_MSGTYPE_AEAD_PRESENT_PUBKEY_CHILD:
   case ZMQ_MSGTYPE_AEAD_ENCINIT:
   case ZMQ_MSGTYPE_AEAD_ENCACK:
   case ZMQ_MSGTYPE_AEAD_REKEY:
   case ZMQ_MSGTYPE_AUTH_CHALLENGE:
   case ZMQ_MSGTYPE_AUTH_REPLY:
   case ZMQ_MSGTYPE_AUTH_PROPOSE:

   {
      return parseMessageWithoutId(dataSlice);
   }

   default:
      LOGERR << "invalid packet type";
   }

   return false;
}

///////////////////////////////////////////////////////////////////////////////
bool ZMQ_BIP15X_Msg::parseSinglePacket(const BinaryDataRef& bdr)
{
   /*
   uint8_t type(ZMQ_MSGTYPE_SINGLEPACKET)
   uint32_t msgid
   nbytes payload
   */

   if (id_ != UINT32_MAX)
      return false;
   BinaryRefReader brr(bdr);

   type_ = brr.get_uint8_t();
   if (type_ != ZMQ_MSGTYPE_SINGLEPACKET)
      return false;

   id_ = brr.get_uint32_t();
   packets_.emplace(make_pair(
      0, brr.get_BinaryDataRef(brr.getSizeRemaining())));

   packetCount_ = 1;
   return true;
}

///////////////////////////////////////////////////////////////////////////////
bool ZMQ_BIP15X_Msg::parseFragmentedMessageHeader(
   const BinaryDataRef& bdr)
{
   /*
   uint8_t type (ZMQ_MSGTYPE_FRAGMENTEDPACKET_HEADER)
   uint32_t msgid
   uint16_t count (>= 2)
   nbytes payload fragment
   */

   BinaryRefReader brr(bdr);

   type_ = brr.get_uint8_t();
   if (type_ != ZMQ_MSGTYPE_FRAGMENTEDPACKET_HEADER)
      return false;

   auto id = brr.get_uint32_t();
   if (id_ != UINT32_MAX && id_ != id)
      return false;
   id_ = id;

   packetCount_ = brr.get_uint16_t();
   packets_.emplace(make_pair(
      0, brr.get_BinaryDataRef(brr.getSizeRemaining())));

   return true;
}

///////////////////////////////////////////////////////////////////////////////
bool ZMQ_BIP15X_Msg::parseMessageFragment(const BinaryDataRef& bdr)
{
   /*
   uint8_t type (ZMQ_MSGTYPE_FRAGMENTEDPACKET_FRAGMENT)
   uint32_t msgid
   varint packet id (1 to 65535)
   nbytes payload fragment
   */

   BinaryRefReader brr(bdr);

   auto type = brr.get_uint8_t();
   if (type != ZMQ_MSGTYPE_FRAGMENTEDPACKET_FRAGMENT)
      return false;

   auto id = brr.get_uint32_t();
   if (id_ != UINT32_MAX && id_ != id)
      return false;
   id_ = id;

   auto packetId = (uint16_t)brr.get_var_int();
   packets_.emplace(make_pair(
      packetId, brr.get_BinaryDataRef(brr.getSizeRemaining())));

   return true;
}

///////////////////////////////////////////////////////////////////////////////
bool ZMQ_BIP15X_Msg::parseMessageWithoutId(const BinaryDataRef& bdr)
{
   /*
   uint8_t type
   nbytes payload
   */

   BinaryRefReader brr(bdr);

   type_ = brr.get_uint8_t();
   if (type_ < ZMQ_MSGTYPE_AEAD_THESHOLD)
      return false;

   packets_.emplace(make_pair(
      0, brr.get_BinaryDataRef(brr.getSizeRemaining())));

   packetCount_ = 1;
   return true;
}

////////////////////////////////////////////////////////////////////////////////
vector<BinaryData> ZMQ_BIP15X_Msg::serializePacketWithoutId(
   const BinaryDataRef& payload, BIP151Connection* connPtr, uint8_t type)
{
   /***
   no packet fragmentation, flat size serialization:
   uint32_t size
   uint8_t type
   nbytes payload
   ***/

   uint32_t size = payload.getSize() + 1;
   BinaryData plainText(4 + size + POLY1305MACLEN);
   if (plainText.getSize() > ZMQ_MESSAGE_PACKET_SIZE)
      throw runtime_error("payload is too large to serialize");

   //skip LWS_PRE, copy in packet size
   memcpy(plainText.getPtr(), &size, 4);
   size += 4;

   //type
   plainText.getPtr()[4] = type;

   //payload
   memcpy(plainText.getPtr() + 5, payload.getPtr(), payload.getSize());

   //encrypt if possible
   vector<BinaryData> result;
   if (connPtr != nullptr)
   {
      connPtr->assemblePacket(plainText.getPtr(), size, plainText.getPtr()
         , size + POLY1305MACLEN);
   }
   else
   {
      plainText.resize(size);
   }

   result.emplace_back(plainText);
   return result;
}

vector<BinaryData> ZMQ_BIP15X_Msg::serialize(const vector<uint8_t>& payload
   , BIP151Connection* connPtr, uint8_t type, uint32_t id){
   BinaryDataRef bdr;
   if(payload.size() > 0)
      bdr.setRef(&payload[0], payload.size());
   return serialize(bdr, connPtr, type, id);
}

////////////////////////////////////////////////////////////////////////////////
vector<BinaryData> ZMQ_BIP15X_Msg::serialize(const string& payload
   , BIP151Connection* connPtr, uint8_t type, uint32_t id) {
   BinaryDataRef bdr((uint8_t*)payload.c_str(), payload.size());
   return serialize(bdr, connPtr, type, id);
}

////////////////////////////////////////////////////////////////////////////////
vector<BinaryData> ZMQ_BIP15X_Msg::serialize(const BinaryDataRef& payload
   , BIP151Connection* connPtr, uint8_t type, uint32_t id) {
   //is this payload carrying a msgid?
   if (type > ZMQ_MSGTYPE_AEAD_THESHOLD)
      return serializePacketWithoutId(payload, connPtr, type);

   /***
   Fragmented packet seralization

   If the payload is less than (WEBSOCKET_MESSAGE_PACKET_SIZE - 9 - LWS_PRE -
   POLY1305MACLEN), use:
    Single packet header:
     uint32_t packet size
     uint8_t type (WS_MSGTYPE_SINGLEPACKET)
     uint32_t msgid
     nbytes payload

   Otherwise, use:
    Fragmented header:
     uint32_t packet size
     uint8_t type (WS_MSGTYPE_FRAGMENTEDPACKET_HEADER)
     uint32_t msgid
     uint16_t count (>= 2)
     nbytes payload fragment

    Fragments:
     uint32_t packet size
     uint8_t type (WS_MSGTYPE_FRAGMENTEDPACKET_FRAGMENT)
     uint32_t msgid
     varint packet id (1 to 65535)
     nbytes payload fragment
   ***/

   //encrypt lambda
   vector<BinaryData> result;
   auto encryptAndAdd = [connPtr, &result](BinaryData& data)
   {
      size_t plainTextLen = data.getSize() - POLY1305MACLEN;
      size_t cipherTextLen = data.getSize();

      if (connPtr != nullptr)
      {
         if (connPtr->assemblePacket(
            data.getPtr(), plainTextLen,
            data.getPtr(), cipherTextLen) != 0)
         {
            //failed to encrypt, abort
            throw runtime_error("failed to encrypt packet, aborting");
         }
      }
      else
      {
         data.resize(cipherTextLen);
      }

      result.emplace_back(data);
   };

   auto data_len = payload.getSize();
   static size_t payload_room =
      ZMQ_MESSAGE_PACKET_SIZE - POLY1305MACLEN - 9;
   if (data_len <= payload_room)
   {
      //single packet serialization
      uint32_t size = data_len + 5;
      BinaryData plainText(POLY1305MACLEN + 9 + data_len);

      memcpy(plainText.getPtr(), &size, 4);
      plainText.getPtr()[4] = ZMQ_MSGTYPE_SINGLEPACKET;
      memcpy(plainText.getPtr() + 5, &id, 4);
      memcpy(plainText.getPtr() + 9, payload.getPtr(), data_len);

      encryptAndAdd(plainText);
   }
   else
   {
      cout << "DEBUG: Fragmented send - We should not get here." << endl;
/*      //2 extra bytes for fragment count
      uint32_t header_room = payload_room - 2;
      size_t left_over = data_len - header_room;

      //1 extra bytes for fragment count < 253
      size_t fragment_room = payload_room - 1;
      uint16_t fragment_count = left_over / fragment_room + 1;
      if (fragment_count >= 253)
      {
         left_over -= fragment_count * fragment_room;

         //3 extra bytes for fragment count >= 253
         fragment_room = payload_room - 3;
         fragment_count += left_over / fragment_room;
      }

      if (left_over % fragment_room != 0)
         ++fragment_count;

      if (fragment_count > 65535)
         throw runtime_error("payload too large for serialization");

      BinaryData header_packet(WEBSOCKET_MESSAGE_PACKET_SIZE);

      //-2 for fragment count
      size_t pos = payload_room - 2;

      //+4 to shave off payload size, +1 for type
      header_room = payload_room + 5;

      memcpy(header_packet.getPtr() + LWS_PRE, &header_room, 4);
      header_packet.getPtr()[LWS_PRE + 4] = WS_MSGTYPE_FRAGMENTEDPACKET_HEADER;
      memcpy(header_packet.getPtr() + LWS_PRE + 5, &id, 4);
      memcpy(header_packet.getPtr() + LWS_PRE + 9, &fragment_count, 2);
      memcpy(header_packet.getPtr() + LWS_PRE + 11, payload.getPtr(), pos);
      encryptAndAdd(header_packet);

      size_t fragment_overhead = 10 + LWS_PRE + POLY1305MACLEN;
      for (unsigned i = 1; i < fragment_count; i++)
      {
         if (i == 253)
            fragment_overhead += 3;

         //figure out data size
         size_t data_size = min(
            WEBSOCKET_MESSAGE_PACKET_SIZE - fragment_overhead,
            data_len - pos);

         BinaryData fragment_packet(data_size + fragment_overhead);
         uint32_t packet_size =
            data_size + fragment_overhead - LWS_PRE - POLY1305MACLEN - 4;

         memcpy(fragment_packet.getPtr() + LWS_PRE, &packet_size, 4);
         fragment_packet.getPtr()[LWS_PRE + 4] = WS_MSGTYPE_FRAGMENTEDPACKET_FRAGMENT;
         memcpy(fragment_packet.getPtr() + LWS_PRE + 5, &id, 4);

         size_t offset = LWS_PRE + 9;
         if (i < 253)
         {
            uint8_t frag_id = i;
            memcpy(fragment_packet.getPtr() + offset, &frag_id, 1);
            ++offset;
         }
         else
         {
            uint16_t frag_id = i;
            fragment_packet.getPtr()[offset++] = 0xFD;
            memcpy(fragment_packet.getPtr() + offset, &frag_id, 2);
            offset += 2;
         }

         memcpy(fragment_packet.getPtr() + offset, payload.getPtr() + pos, data_size);
         pos += data_size;

         encryptAndAdd(fragment_packet);
      }*/
   }

   return result;
}

///////////////////////////////////////////////////////////////////////////////
bool ZMQ_BIP15X_Msg::isReady() const
{
   return packets_.size() == packetCount_;
}

///////////////////////////////////////////////////////////////////////////////
uint8_t ZMQ_BIP15X_Msg::getPacketType(const BinaryDataRef& bdr)
{
   if (bdr.getSize() < 5) {
      throw runtime_error("packet is too small to be serialized fragment");
   }

   return bdr.getPtr()[4];
}

///////////////////////////////////////////////////////////////////////////////
BinaryDataRef ZMQ_BIP15X_Msg::getSingleBinaryMessage() const {
   if (packetCount_ != 1 || !isReady()) {
      return BinaryDataRef();
   }

   return packets_.begin()->second;
}
