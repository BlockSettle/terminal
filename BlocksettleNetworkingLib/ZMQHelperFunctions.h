#ifndef __ZMQ_HELPER_FUNCTIONS_H__
#define __ZMQ_HELPER_FUNCTIONS_H__

#include <string>
#include <QString>
#include <spdlog/spdlog.h>
#include "EncryptionUtils.h"

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
