#ifndef __ZMQ_HELPER_FUNCTIONS_H__
#define __ZMQ_HELPER_FUNCTIONS_H__

#include <string>

namespace bs
{
   namespace network
   {
      int get_monitor_event(void *monitor);
      int get_monitor_event(void *monitor, int *value);
      std::string peerAddressString(int socket);
   }
}

#endif // __ZMQ_HELPER_FUNCTIONS_H__
