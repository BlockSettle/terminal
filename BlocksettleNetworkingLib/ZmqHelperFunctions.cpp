/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "ZmqHelperFunctions.h"
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
