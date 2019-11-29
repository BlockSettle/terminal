/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef ZMQ_BIP15X_MSG_H
#define ZMQ_BIP15X_MSG_H

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
// Note that fragments need not be reassembled before decrypting them. It is
// important to note the packet number and parse out the decrypted fragments in
// order. Otherwise, the fragments aren't special in terms of handling. That
// said, to make things easier, it's best to run a map of them through
// ZmqBIP15XMessageCodec, which can output single messages for fragmented and
// non-fragmented.

constexpr uint8_t ZMQ_MSGTYPE_SINGLEPACKET          = 1;

constexpr uint8_t ZMQ_MSGTYPE_AEAD_THRESHOLD        = 10;
constexpr uint8_t ZMQ_MSGTYPE_AEAD_SETUP            = 11;
constexpr uint8_t ZMQ_MSGTYPE_AEAD_PRESENT_PUBKEY   = 12;
constexpr uint8_t ZMQ_MSGTYPE_AEAD_ENCINIT          = 14;
constexpr uint8_t ZMQ_MSGTYPE_AEAD_ENCACK           = 15;
constexpr uint8_t ZMQ_MSGTYPE_AEAD_REKEY            = 16;

constexpr uint8_t ZMQ_MSGTYPE_AUTH_THESHOLD         = 20;
constexpr uint8_t ZMQ_MSGTYPE_AUTH_CHALLENGE        = 21;
constexpr uint8_t ZMQ_MSGTYPE_AUTH_REPLY            = 22;
constexpr uint8_t ZMQ_MSGTYPE_AUTH_PROPOSE          = 23;

// Not part of the standard - Required by ZMQ.
constexpr uint8_t ZMQ_MSGTYPE_HEARTBEAT             = 30;
constexpr uint8_t ZMQ_MSGTYPE_DISCONNECT            = 31;

constexpr int ZMQ_AEAD_REKEY_INVERVAL_SECS = 600;

// A class used to represent messages on the wire that need to be created.
class ZmqBipMsgBuilder
{
private:
   BinaryData packet_;

   void construct(const uint8_t *data, int dataSize, uint8_t type);
public:
   // Constructs plain packet
   ZmqBipMsgBuilder(const uint8_t *data, int dataSize, uint8_t type);

   // Shotcuts for the first ctor
   ZmqBipMsgBuilder(const std::vector<uint8_t>& data, uint8_t type);
   ZmqBipMsgBuilder(const BinaryDataRef& data, uint8_t type);
   ZmqBipMsgBuilder(const std::string& data, uint8_t type);

   // Constructs plain packet without data
   ZmqBipMsgBuilder(uint8_t type);

   // Encrypts plain packet. If conn is not set this is NOOP.
   ZmqBipMsgBuilder& encryptIfNeeded(BIP151Connection *conn);

   // Returns packet that is ready for send
   BinaryData build();
};

// A class used to represent messages on the wire that need to be decrypted.
class ZmqBipMsg
{
private:
   BinaryDataRef data_;
   uint8_t type_{0};

   ZmqBipMsg() = default;
public:
   // Parse and return immutable message.
   // Message might be invalid if parsing failed (check with isValid)
   // Does not copy underlying raw data, make sure packet is live long enough.
   static ZmqBipMsg parsePacket(const BinaryDataRef& packet);

   // Validate if packet is valid before use
   bool isValid() const { return type_ != 0; }

   // Packet's type (ZMQ_MSGTYPE_SINGLEPACKET, ZMQ_MSGTYPE_HEARTBEAT etc)
   uint8_t getType() const { return type_; }

   // Packet's payload
   BinaryDataRef getData() const { return data_; }
};

#endif // ZMQ_BIP15X_MSG_H
