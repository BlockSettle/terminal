#ifndef __ZMQ_BIP15X_MSG_H__
#define __ZMQ_BIP15X_MSG_H__

// Messages used for ZmqBIP15XDataConnection and ZmqBIP15XServerConnection
// connections.

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
// REMAINING DATA - Fragments (header or pieces)
// Set aside for now. Use only if required.

#include <spdlog/spdlog.h>
#include "BinaryData.h"
#include "BIP150_151.h"

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

// NOTE: ZMQ will blast out the message in one packet. This won't work with
// Ethernet but is okay for local connections in memory (loopback). This code
// needs to enable fragmentation, as seen in Armory's WebSockets code. Once
// that's done, this can be set back to 1500 to properly enable network
// connections.
#define ZMQ_MESSAGE_PACKET_SIZE 65000


#define ZMQ_CALLBACK_ID 0xFFFFFFFD
#define ZMQ_AEAD_HANDSHAKE_ID 0xFFFFFFFC
#define ZMQ_MAGIC_WORD 0x56E1
#define AEAD_REKEY_INVERVAL_SECONDS 600

class ZmqBIP15XMsg {
public:
   void reset(void);
   bool parsePacket(const BinaryDataRef&);
   bool isReady(void) const;
   const uint32_t& getId(void) const { return id_; }
   uint8_t getType(void) const { return type_; }

   static std::vector<BinaryData> serialize(const std::vector<uint8_t>& payload
      , BIP151Connection* connPtr, uint8_t type, uint32_t id);
   static std::vector<BinaryData> serialize(const std::string& payload
      , BIP151Connection* connPtr, uint8_t type, uint32_t id);
   static std::vector<BinaryData> serialize(const BinaryDataRef& payload
      , BIP151Connection* connPtr, uint8_t type, uint32_t id);
   static std::vector<BinaryData> serializePacketWithoutId(
      const BinaryDataRef& payload, BIP151Connection* connPtr, uint8_t type);
   BinaryDataRef getSingleBinaryMessage() const;

   const std::map<uint16_t, BinaryDataRef>& getPacketMap(void) const { return packets_; }
   static uint8_t getPacketType(const BinaryDataRef&);

private:
   std::map<uint16_t, BinaryDataRef> packets_;

   uint32_t id_ = UINT32_MAX;
   uint8_t type_ = UINT8_MAX;
   uint32_t packetCount_ = UINT32_MAX;

   bool parseSinglePacket(const BinaryDataRef& bdr);
   bool parseFragmentedMessageHeader(const BinaryDataRef& bdr);
   bool parseMessageFragment(const BinaryDataRef& bdr);
   bool parseMessageWithoutId(const BinaryDataRef& bdr);
};

#endif // __ZMQ_BIP15X_MSG_H__
