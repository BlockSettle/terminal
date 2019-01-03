#include "ZMQHelperFunctions.h"

#include "MessageHolder.h"

#ifndef WIN32
#  include <arpa/inet.h>
#endif

#include <zmq.h>

int bs::network::get_monitor_event(void *monitor)
{
   // First frame in message contains event number and value
   zmq_msg_t msg;
   zmq_msg_init(&msg);
   if (zmq_msg_recv(&msg, monitor, 0) == -1) {
      return -1; // Interruped, presumably
   }
   if (!zmq_msg_more(&msg)) {
      return -1;
   }

   uint8_t *data = (uint8_t *)zmq_msg_data(&msg);
   uint16_t event = *(uint16_t *)(data);

   // Second frame in message contains event address
   zmq_msg_init(&msg);
   if (zmq_msg_recv(&msg, monitor, 0) == -1) {
      return -1; // Interruped, presumably
   }
   if (zmq_msg_more(&msg)) {
      return -1;
   }

   return event;
}

int bs::network::get_monitor_event(void *monitor, int *value)
{
   // First frame in message contains event number and value
   zmq_msg_t msg;
   zmq_msg_init(&msg);
   if (zmq_msg_recv(&msg, monitor, 0) == -1) {
      return -1; // Interruped, presumably
   }

   uint8_t *data = (uint8_t *) zmq_msg_data(&msg);
   uint16_t event = *(uint16_t *)(data);
   if (value) {
      *value = *(uint32_t *)(data + 2);
   }

   // Second frame in message contains event address
   zmq_msg_init(&msg);
   if (zmq_msg_recv(&msg, monitor, 0) == -1) {
      return -1; // Interruped, presumably
   }

   return event;
}

std::string bs::network::peerAddressString(int socket)
{
#ifdef WIN32
   SOCKET sock = socket;
   SOCKADDR_IN peerInfo = { 0 };
   int peerLen = sizeof(peerInfo);
#else
   int sock = socket;
   sockaddr_in peerInfo = { 0 };
   socklen_t peerLen = sizeof(peerInfo);
#endif

   const auto rc = getpeername(sock, (sockaddr*)&peerInfo, &peerLen);
   if (rc == 0) {
      return inet_ntoa(peerInfo.sin_addr);
   } else {
      return "Not detected";
   }
}

// A function that returns a new CurveZMQ keypair. While each binary key will be
// 32 bytes, the keys will be returned Z85-encoded and will be 41 bytes (40 byte
// Z85-encoded string + null char).
// IN:     Logger object (spdlog::logger pointer)
// OUT:    Public/Private key pair buffer (<BinaryData, SecureBinaryData>)
// RETURN: -2 (Setup failure), -1 (ZMQ failure), 0 (Success)
int bs::network::getCurveZMQKeyPair(std::pair<BinaryData, SecureBinaryData>& outKeyPair)
{
   BinaryData pubKey(CURVEZMQPUBKEYBUFFERSIZE);
   SecureBinaryData prvKey(CURVEZMQPRVKEYBUFFERSIZE);

   // Generate the keypair and overwrite the incoming pair.
   int retVal = zmq_curve_keypair(pubKey.getCharPtr(), prvKey.getCharPtr());
   outKeyPair = std::make_pair(pubKey, prvKey);

   return retVal;
}
