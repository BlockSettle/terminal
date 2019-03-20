#include "ZMQ_BIP15X_Msg.h"

using namespace std;

// NB: Data is mostly copied from Armory's WebSocket code, with mods as needed.

// Reset the message.
//
// INPUT:  None
// OUTPUT: None
// RETURN: None
void ZmqBIP15XMsg::reset()
{
   packets_.clear();
   id_ = UINT32_MAX;
   type_ = UINT8_MAX;
   packetCount_ = UINT32_MAX;
}

// Parse incoming raw data into a message.
//
// INPUT:  The raw data. (const BinaryDataRef&)
// OUTPUT: None
// RETURN: True if success, false if failure.
bool ZmqBIP15XMsg::parsePacket(const BinaryDataRef& dataRef)
{
   if (dataRef.getSize() == 0)
      return false;

   BinaryRefReader brrPacket(dataRef);
   auto packetlen = brrPacket.get_uint32_t();
   if (packetlen != brrPacket.getSizeRemaining()) {
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
      break;
   }

   return false;
}

// Parse a packet with all data in one packet.
//
// INPUT:  The raw data. (const BinaryDataRef&)
// OUTPUT: None
// RETURN: True if success, false if failure.
bool ZmqBIP15XMsg::parseSinglePacket(const BinaryDataRef& bdr)
{
   /*
   uint32_t msgid
   nbytes payload
   */

   if (id_ != UINT32_MAX) {
      return false;
   }
   BinaryRefReader brr(bdr);

   type_ = brr.get_uint8_t();
   if (type_ != ZMQ_MSGTYPE_SINGLEPACKET) {
      return false;
   }

   id_ = brr.get_uint32_t();
   packets_.emplace(make_pair(
      0, brr.get_BinaryDataRef(brr.getSizeRemaining())));

   packetCount_ = 1;
   return true;
}

// Parse a fragmented packet header.
//
// INPUT:  The raw data header. (const BinaryDataRef&)
// OUTPUT: None
// RETURN: True if success, false if failure.
bool ZmqBIP15XMsg::parseFragmentedMessageHeader(
   const BinaryDataRef& bdr)
{
   /*
   uint32_t msgid
   uint16_t count (>= 2)
   nbytes payload fragment
   */

   BinaryRefReader brr(bdr);

   type_ = brr.get_uint8_t();
   if (type_ != ZMQ_MSGTYPE_FRAGMENTEDPACKET_HEADER) {
      return false;
   }

   auto id = brr.get_uint32_t();
   if (id_ != UINT32_MAX && id_ != id) {
      return false;
   }
   id_ = id;

   packetCount_ = brr.get_uint16_t();
   packets_.emplace(make_pair(
      0, brr.get_BinaryDataRef(brr.getSizeRemaining())));

   return true;
}

// Parse a fragmented packet piece.
//
// INPUT:  The raw data header. (const BinaryDataRef&)
// OUTPUT: None
// RETURN: True if success, false if failure.
bool ZmqBIP15XMsg::parseMessageFragment(const BinaryDataRef& bdr)
{
   /*
   uint32_t msgid
   varint packet id (1 to 65535)
   nbytes payload fragment
   */

   BinaryRefReader brr(bdr);

   auto type = brr.get_uint8_t();
   if (type != ZMQ_MSGTYPE_FRAGMENTEDPACKET_FRAGMENT) {
      return false;
   }

   auto id = brr.get_uint32_t();
   if (id_ != UINT32_MAX && id_ != id) {
      return false;
   }
   id_ = id;

   auto packetId = (uint16_t)brr.get_var_int();
   packets_.emplace(make_pair(
      packetId, brr.get_BinaryDataRef(brr.getSizeRemaining())));

   return true;
}

// Parse a single packet with no ID.
//
// INPUT:  The raw data. (const BinaryDataRef&)
// OUTPUT: None
// RETURN: True if success, false if failure.
bool ZmqBIP15XMsg::parseMessageWithoutId(const BinaryDataRef& bdr)
{
   /*
   uint8_t type
   nbytes payload
   */

   BinaryRefReader brr(bdr);

   type_ = brr.get_uint8_t();
   if (type_ < ZMQ_MSGTYPE_AEAD_THRESHOLD) {
      return false;
   }

   packets_.emplace(make_pair(0, brr.get_BinaryDataRef(brr.getSizeRemaining())));

   packetCount_ = 1;
   return true;
}

// Create a single packet with no ID.
//
// INPUT:  The raw payload. (const BinaryDataRef&)
//         The BIP 150/151 handshake data. (BIP151Connection*)
//         The packet type. (uint8_t)
// OUTPUT: None
// RETURN: A vector with the packets. Will usually have only one entry.
vector<BinaryData> ZmqBIP15XMsg::serializePacketWithoutId(
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
   if (plainText.getSize() > ZMQ_MESSAGE_PACKET_SIZE) {
      throw runtime_error("payload is too large to serialize");
   }

   //skip LWS_PRE, copy in packet size
   memcpy(plainText.getPtr(), &size, 4);
   size += 4;

   //type
   plainText.getPtr()[4] = type;

   //payload
   memcpy(plainText.getPtr() + 5, payload.getPtr(), payload.getSize());

   //encrypt if possible
   vector<BinaryData> result;
   if (connPtr != nullptr) {
      connPtr->assemblePacket(plainText.getPtr(), size, plainText.getPtr()
         , size + POLY1305MACLEN);
   }
   else {
      plainText.resize(size);
   }

   result.emplace_back(plainText);
   return result;
}

vector<BinaryData> ZmqBIP15XMsg::serialize(const vector<uint8_t>& payload
   , BIP151Connection* connPtr, uint8_t type, uint32_t id)
{
   BinaryDataRef bdr;
   if (payload.size() > 0) {
      bdr.setRef(&payload[0], payload.size());
   }
   return serialize(bdr, connPtr, type, id);
}

// Packet serialization frontend.
//
// INPUT:  The raw payload. (const string&)
//         The BIP 150/151 handshake data. (BIP151Connection*)
//         The packet type. (uint8_t)
//         Message type. (uint32_t)
// OUTPUT: None
// RETURN: A vector with the packets. Will usually have only one entry.
vector<BinaryData> ZmqBIP15XMsg::serialize(const string& payload
   , BIP151Connection* connPtr, uint8_t type, uint32_t id)
{
   BinaryDataRef bdr((uint8_t*)payload.c_str(), payload.size());
   return serialize(bdr, connPtr, type, id);
}

// Packet serialization frontend. Note that, for now, fragmented packets aren't
// supported. This shouldn't be a problem for ZMQ, as it seems to generate
// single messages from fragments on-the-wire. For now, some code from Armory
// that supports fragmentation will be left in, just in case it's needed later.
// The code can always be removed later. (Armory uses WebSocket, which doesn't
// automatically process on-the-wire fragments.
//
// INPUT:  The raw payload. (const BinaryDataRef&)
//         The BIP 150/151 handshake data. (BIP151Connection*)
//         The packet type. Usually ZMQ_MSGTYPE_SINGLEPACKET. (uint8_t)
//         Message type. Usually 0. (uint32_t)
// OUTPUT: None
// RETURN: A vector with the packets. Will usually have only one entry.
vector<BinaryData> ZmqBIP15XMsg::serialize(const BinaryDataRef& payload
   , BIP151Connection* connPtr, uint8_t type, uint32_t id)
{
   //is this payload carrying a msgid?
   if (type > ZMQ_MSGTYPE_AEAD_THRESHOLD) {
      return serializePacketWithoutId(payload, connPtr, type);
   }

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
   auto encryptAndAdd = [connPtr, &result](BinaryData& data) {
      size_t plainTextLen = data.getSize() - POLY1305MACLEN;
      size_t cipherTextLen = data.getSize();

      if (connPtr != nullptr) {
         if (connPtr->assemblePacket(
            data.getPtr(), plainTextLen,
            data.getPtr(), cipherTextLen) != 0) {
            //failed to encrypt, abort
            throw runtime_error("failed to encrypt packet, aborting");
         }
      }
      else {
         data.resize(cipherTextLen);
      }

      result.emplace_back(data);
   };

   auto dataLen = payload.getSize();
   static size_t payloadRoom =
      ZMQ_MESSAGE_PACKET_SIZE - POLY1305MACLEN - 9;
   if (dataLen <= payloadRoom) {
      //single packet serialization
      uint32_t size = dataLen + 5;
      BinaryData plainText(POLY1305MACLEN + 9 + dataLen);

      memcpy(plainText.getPtr(), &size, 4);
      plainText.getPtr()[4] = ZMQ_MSGTYPE_SINGLEPACKET;
      memcpy(plainText.getPtr() + 5, &id, 4);
      memcpy(plainText.getPtr() + 9, payload.getPtr(), dataLen);

      encryptAndAdd(plainText);
   }
   else {
/*      //2 extra bytes for fragment count
      uint32_t headerRoom = payloadRoom - 2;
      size_t leftOver = dataLen - headerRoom;

      //1 extra bytes for fragment count < 253
      size_t fragmentRoom = payloadRoom - 1;
      uint16_t fragmentCount = leftOver / fragmentRoom + 1;
      if (fragmentCount >= 253)
      {
         leftOver -= fragmentCount * fragmentRoom;

         //3 extra bytes for fragment count >= 253
         fragmentRoom = payloadRoom - 3;
         fragmentCount += leftOver / fragmentRoom;
      }

      if (leftOver % fragmentRoom != 0)
         ++fragmentCount;

      if (fragmentCount > 65535)
         throw runtime_error("payload too large for serialization");

      BinaryData headerPacket(WEBSOCKET_MESSAGE_PACKET_SIZE);

      //-2 for fragment count
      size_t pos = payloadRoom - 2;

      //+4 to shave off payload size, +1 for type
      headerRoom = payloadRoom + 5;

      memcpy(headerPacket.getPtr() + LWS_PRE, &headerRoom, 4);
      headerPacket.getPtr()[LWS_PRE + 4] = WS_MSGTYPE_FRAGMENTEDPACKET_HEADER;
      memcpy(headerPacket.getPtr() + LWS_PRE + 5, &id, 4);
      memcpy(headerPacket.getPtr() + LWS_PRE + 9, &fragmentCount, 2);
      memcpy(headerPacket.getPtr() + LWS_PRE + 11, payload.getPtr(), pos);
      encryptAndAdd(headerPacket);

      size_t fragmentOverhead = 10 + LWS_PRE + POLY1305MACLEN;
      for (unsigned i = 1; i < fragmentCount; i++)
      {
         if (i == 253)
            fragmentOverhead += 3;

         //figure out data size
         size_t dataSize = min(
            WEBSOCKET_MESSAGE_PACKET_SIZE - fragmentOverhead,
            dataLen - pos);

         BinaryData fragmentPacket(dataSize + fragmentOverhead);
         uint32_t packetSize =
            dataSize + fragmentOverhead - LWS_PRE - POLY1305MACLEN - 4;

         memcpy(fragmentPacket.getPtr() + LWS_PRE, &packetSize, 4);
         fragmentPacket.getPtr()[LWS_PRE + 4] = WS_MSGTYPE_FRAGMENTEDPACKET_FRAGMENT;
         memcpy(fragmentPacket.getPtr() + LWS_PRE + 5, &id, 4);

         size_t offset = LWS_PRE + 9;
         if (i < 253)
         {
            uint8_t fragID = i;
            memcpy(fragmentPacket.getPtr() + offset, &fragID, 1);
            ++offset;
         }
         else
         {
            uint16_t fragID = i;
            fragmentPacket.getPtr()[offset++] = 0xFD;
            memcpy(fragmentPacket.getPtr() + offset, &fragID, 2);
            offset += 2;
         }

         memcpy(fragmentPacket.getPtr() + offset, payload.getPtr() + pos, dataSize);
         pos += dataSize;

         encryptAndAdd(fragmentPacket);
      }*/
   }

   return result;
}

// Function indicating if a packet is ready to send. Meant primarily for
// fragmented packets.
//
// INPUT:  None
// OUTPUT: None
// RETURN: True if ready, false if not.
bool ZmqBIP15XMsg::isReady() const
{
   return packets_.size() == packetCount_;
}

// Function returning the packet type.
//
// INPUT:  The packet data. (const BinaryDataRef&)
// OUTPUT: None
// RETURN: The packet type. (uint8_t)
uint8_t ZmqBIP15XMsg::getPacketType(const BinaryDataRef& bdr)
{
   if (bdr.getSize() < 5) {
      throw runtime_error("packet is too small to be a serialized fragment");
   }

   return bdr.getPtr()[4];
}

// Function returning the serialized packet data.
//
// INPUT:  None
// OUTPUT: None
// RETURN: The packet data. (BinaryDataRef)
BinaryDataRef ZmqBIP15XMsg::getSingleBinaryMessage() const
{
   if (packetCount_ != 1 || !isReady()) {
      return BinaryDataRef();
   }

   return packets_.begin()->second;
}
