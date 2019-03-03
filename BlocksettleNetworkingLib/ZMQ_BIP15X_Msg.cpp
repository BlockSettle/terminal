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

///////////////////////////////////////////////////////////////////////////////
bool ZMQ_BIP15X_Msg::isReady() const
{
   return packets_.size() == packetCount_;
}

///////////////////////////////////////////////////////////////////////////////
uint8_t ZMQ_BIP15X_Msg::getPacketType(const BinaryDataRef& bdr)
{
   if (bdr.getSize() < 5)
      throw runtime_error("packet is too small to be serialized fragment");
   return bdr.getPtr()[4];
}
