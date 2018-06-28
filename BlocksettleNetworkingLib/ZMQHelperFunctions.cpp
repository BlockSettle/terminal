#include "ZMQHelperFunctions.h"

#include "MessageHolder.h"

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