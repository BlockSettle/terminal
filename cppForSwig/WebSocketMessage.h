////////////////////////////////////////////////////////////////////////////////
//                                                                            //
//  Copyright (C) 2018, goatpig.                                              //
//  Distributed under the MIT license                                         //
//  See LICENSE-MIT or https://opensource.org/licenses/MIT                    //                                      
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#ifndef _WEBSOCKET_MESSAGE_H
#define _WEBSOCKET_MESSAGE_H

#include <stdexcept>
#include <string>
#include <memory>

#include "BinaryData.h"
#include <google/protobuf/message.h>
#include "SocketObject.h"

#include "BIP150_151.h"

#define WEBSOCKET_MESSAGE_PACKET_SIZE 1500
#define WEBSOCKET_CALLBACK_ID 0xFFFFFFFE
#define WEBSOCKET_AEAD_HANDSHAKE_ID 0xFFFFFFFD
#define WEBSOCKET_MAGIC_WORD 0x56E1
#define AEAD_REKEY_INVERVAL_SECONDS 600

#define WS_MSGTYPE_SINGLEPACKET              1
#define WS_MSGTYPE_FRAGMENTEDPACKET_HEADER   2
#define WS_MSGTYPE_FRAGMENTEDPACKET_FRAGMENT 3

#define WS_MSGTYPE_AEAD_THESHOLD             10
#define WS_MSGTYPE_AEAD_SETUP                11
#define WS_MSGTYPE_AEAD_PRESENT_PUBKEY       12
#define WS_MSGTYPE_AEAD_PRESENT_PUBKEY_CHILD 13
#define WS_MSGTYPE_AEAD_ENCINIT              14
#define WS_MSGTYPE_AEAD_ENCACK               15
#define WS_MSGTYPE_AEAD_REKEY                16

#define WS_MSGTYPE_AUTH_THESHOLD             20
#define WS_MSGTYPE_AUTH_CHALLENGE            21
#define WS_MSGTYPE_AUTH_REPLY                22
#define WS_MSGTYPE_AUTH_PROPOSE              23

class LWS_Error : public std::runtime_error
{
public:
   LWS_Error(const std::string& err) :
      std::runtime_error(err)
   {}
};

///////////////////////////////////////////////////////////////////////////////
class WebSocketMessageCodec
{
public:
   static std::vector<BinaryData> serialize(
      const BinaryDataRef&, BIP151Connection*, uint8_t, uint32_t);
   static std::vector<BinaryData> serialize(
      const std::vector<uint8_t>&, BIP151Connection*, uint8_t, uint32_t);
   static std::vector<BinaryData> serialize(
      const std::string&, BIP151Connection*, uint8_t, uint32_t);
   static std::vector<BinaryData> serializePacketWithoutId(
      const BinaryDataRef&, BIP151Connection*, uint8_t);

   static uint32_t getMessageId(const BinaryDataRef&);
    
   static bool reconstructFragmentedMessage(
      const std::map<uint16_t, BinaryDataRef>&, 
      ::google::protobuf::Message*);
};

///////////////////////////////////////////////////////////////////////////////
class SerializedMessage
{
private:
   mutable unsigned index_ = 0;
   std::vector<BinaryData> packets_;

public:
   SerializedMessage()
   {}

   void construct(const std::vector<uint8_t>& data, BIP151Connection*,
      uint8_t, uint32_t id = 0);
   void construct(const BinaryDataRef& data, BIP151Connection*,
      uint8_t, uint32_t id = 0);

   bool isDone(void) const { return index_ >= packets_.size(); }
   const BinaryData& getNextPacket(void) const;
};

///////////////////////////////////////////////////////////////////////////////
class WebSocketMessagePartial
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
   bool getMessage(::google::protobuf::Message*) const;
   BinaryDataRef getSingleBinaryMessage(void) const;
   const uint32_t& getId(void) const { return id_; }
   uint8_t getType(void) const { return type_; }

   const std::map<uint16_t, BinaryDataRef>& getPacketMap(void) const { return packets_; }
   static uint8_t getPacketType(const BinaryDataRef&);
};

///////////////////////////////////////////////////////////////////////////////
class CallbackReturn_WebSocket : public CallbackReturn
{
private:
   void callback(BinaryDataRef bdr) {}

public:
   virtual void callback(const WebSocketMessagePartial&) = 0;
};

#endif