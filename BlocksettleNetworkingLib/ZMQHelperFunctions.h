#ifndef __ZMQ_HELPER_FUNCTIONS_H__
#define __ZMQ_HELPER_FUNCTIONS_H__

#include <string>
#include <QString>
#include <spdlog/spdlog.h>
#include "EncryptionUtils.h"

#define CURVEZMQPUBKEYBUFFERSIZE 40
#define CURVEZMQPRVKEYBUFFERSIZE 40

namespace bs
{
   namespace network
   {
      int get_monitor_event(void *monitor);
      int get_monitor_event(void *monitor, int *value);
      std::string peerAddressString(int socket);
      int getCurveZMQKeyPair(std::pair<SecureBinaryData, SecureBinaryData>& keyPair);
      bool readZmqKeyFile(const QString& zmqKeyFilePath
         , SecureBinaryData& zmqKey, const bool& isPub
         , const std::shared_ptr<spdlog::logger>& logger = nullptr);

      bool readZmqKeyString(const QByteArray& zmqEncodedKey
         , SecureBinaryData& zmqKey, const bool& isPub
         , const std::shared_ptr<spdlog::logger>& logger = nullptr);
   }
}

#endif // __ZMQ_HELPER_FUNCTIONS_H__
