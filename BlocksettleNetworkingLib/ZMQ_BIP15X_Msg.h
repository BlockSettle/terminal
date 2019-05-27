#ifndef __ZMQ_BIP15X_MSG_H__
#define __ZMQ_BIP15X_MSG_H__

// Messages used for ZmqBIP15XDataConnection and ZmqBIP15XServerConnection
// connections.

#include <spdlog/spdlog.h>
#include "BinaryData.h"
#include "BIP150_151.h"

// The message format is as follows:
//
// Packet length (4 bytes)
// Message type (1 byte)
// Remaining data - Depends.
//
// REMAINING DATA - Single packet
// Message ID (4 bytes - Not in BIP 150/151 handshake packets)
// Payload  (N bytes)
//
// REMAINING DATA - Fragments (header / fragment 0)
// Message ID (4 bytes)
// Packet count (2 bytes)
// First fragment payload  (N bytes)
//
// REMAINING DATA - Fragments (fragment piece 1-65535)
// Message ID (4 bytes)
// Packet number (2 bytes - 1-65535)
// Payload  (N bytes)
//
// Note that fragments need not be reassembled before decrypting them. It is
// important to note the packet number and parse out the decrypted fragments in
// order. Otherwise, the fragments aren't special in terms of handling. That
// said, to make things easier, it's best to run a map of them through
// ZmqBIP15XMessageCodec, which can output single messages for fragmented and
// non-fragmented.

#define ZMQ_MSGTYPE_SINGLEPACKET              1
#define ZMQ_MSGTYPE_FRAGMENTEDPACKET_HEADER   2
#define ZMQ_MSGTYPE_FRAGMENTEDPACKET_FRAGMENT 3

#define ZMQ_MSGTYPE_AEAD_THRESHOLD            10
#define ZMQ_MSGTYPE_AEAD_SETUP                11
#define ZMQ_MSGTYPE_AEAD_PRESENT_PUBKEY       12
#define ZMQ_MSGTYPE_AEAD_PRESENT_PUBKEY_CHILD 13
#define ZMQ_MSGTYPE_AEAD_ENCINIT              14
#define ZMQ_MSGTYPE_AEAD_ENCACK               15
#define ZMQ_MSGTYPE_AEAD_REKEY                16

#define ZMQ_MSGTYPE_AUTH_THESHOLD             20
#define ZMQ_MSGTYPE_AUTH_CHALLENGE            21
#define ZMQ_MSGTYPE_AUTH_REPLY                22
#define ZMQ_MSGTYPE_AUTH_PROPOSE              23

// Not part of the standard - Required by ZMQ.
#define ZMQ_MSGTYPE_HEARTBEAT                 30
#define ZMQ_MSGTYPE_DISCONNECT                31

// NOTE: ZMQ is message-oriented. We must split up messages in order to play
// nice with TCP. Max msg size = 65535 * 1400 = ~92 MiB
#define ZMQ_MESSAGE_PACKET_SIZE 1400

#define ZMQ_CALLBACK_ID 0xFFFFFFFD
#define ZMQ_AEAD_HANDSHAKE_ID 0xFFFFFFFC
#define ZMQ_MAGIC_WORD 0x56E1
#define ZMQ_AEAD_REKEY_INVERVAL_SECS 600

// Class that actually handles the nuts & bolts of message construction.
class ZmqBIP15XMessageCodec
{
public:
   static std::vector<BinaryData> serialize(const BinaryDataRef& payload
      , BIP151Connection* connPtr, uint8_t type, uint32_t id);
   static std::vector<BinaryData> serialize(const std::vector<uint8_t>& payload
      , BIP151Connection* connPtr, uint8_t type, uint32_t id);
   static std::vector<BinaryData> serialize(const std::string& payload
      , BIP151Connection* connPtr, uint8_t type, uint32_t id);
   static std::vector<BinaryData> serializePacketWithoutId(
      const BinaryDataRef& payload, BIP151Connection* connPtr, uint8_t type);

   static uint32_t getMessageId(const BinaryDataRef& packet);

   static bool reconstructFragmentedMessage(
      const std::map<uint16_t, BinaryDataRef>& payload,
      BinaryData* msg);
};

// A class used to represent messages on the wire that need to be created.
class ZmqBIP15XSerializedMessage
{
private:
   mutable unsigned index_ = 0;
   std::vector<BinaryData> packets_;

public:
   ZmqBIP15XSerializedMessage()
   {}

   void construct(const std::vector<uint8_t>& data, BIP151Connection*,
      uint8_t, uint32_t id = 0);
   void construct(const BinaryDataRef& data, BIP151Connection*,
      uint8_t, uint32_t id = 0);

   bool isDone(void) const { return index_ >= packets_.size(); }
   const BinaryData& getNextPacket(void) const;
};

// A class used to represent messages on the wire that need to be decrypted.
class ZmqBIP15XMsgPartial
{
private:
   std::map<uint16_t, BinaryDataRef> packets_;

   uint32_t id_ = UINT32_MAX;
   uint8_t type_ = UINT8_MAX;
   uint32_t packetCount_ = UINT32_MAX;

private:
   bool parseSinglePacket(const BinaryDataRef& bdr);
   bool parseFragmentedMessageHeader(const BinaryDataRef& bdr);
   bool parseMessageFragment(const BinaryDataRef& bdr);
   bool parseMessageWithoutId(const BinaryDataRef& bdr);

public:
   void reset(void);
   bool parsePacket(const BinaryDataRef&);
   bool isReady(void) const;
   void getMessage(BinaryData*) const;
   BinaryDataRef getSingleBinaryMessage(void) const;
   const uint32_t& getId(void) const { return id_; }
   uint8_t getType(void) const { return type_; }

   const std::map<uint16_t, BinaryDataRef>& getPacketMap(void) const { return packets_; }
   static uint8_t getPacketType(const BinaryDataRef&);
};

// A struct containing fragments from a decrypted packet. Can be used to easily
// generate a single, final decrypted packet. Incoming packets must already be
// decrypted.
struct ZmqBIP15XMsgFragments
{
private:
   int counter_ = 0;

public:
   std::map<uint16_t, BinaryData> packets_;
   ZmqBIP15XMsgPartial message_;

   void reset(void);
   BinaryDataRef insertDataAndGetRef(BinaryData& data);
   void eraseLast(void);
};

#endif // __ZMQ_BIP15X_MSG_H__
