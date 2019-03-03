#ifndef __ZMQ_BIP15X_CONNECTION_H__
#define __ZMQ_BIP15X_CONNECTION_H__

#include <spdlog/spdlog.h>
#include "BinaryData.h"

#define ZMQ_MSGTYPE_SINGLEPACKET              1
#define ZMQ_MSGTYPE_FRAGMENTEDPACKET_HEADER   2
#define ZMQ_MSGTYPE_FRAGMENTEDPACKET_FRAGMENT 3

#define ZMQ_MSGTYPE_AEAD_THESHOLD             10
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

#define CLIENT_AUTH_PEER_FILENAME "client.peers"

class ZMQ_BIP15X_Msg {
public:
   void reset(void);
   bool parsePacket(const BinaryDataRef&);
   bool isReady(void) const;
   const uint32_t& getId(void) const { return id_; }
   uint8_t getType(void) const { return type_; }

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

#endif // __ZMQ_BIP15X_CONNECTION_H__
