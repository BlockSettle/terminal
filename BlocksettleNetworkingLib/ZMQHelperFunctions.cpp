#include <QFile>

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
int bs::network::getCurveZMQKeyPair(std::pair<SecureBinaryData, SecureBinaryData>& outKeyPair)
{
   char pubKey[CURVEZMQPUBKEYBUFFERSIZE + 1];
   char prvKey[CURVEZMQPRVKEYBUFFERSIZE + 1];

   // Generate the keypair and overwrite the incoming pair.
   int retVal = zmq_curve_keypair(pubKey, prvKey);
   outKeyPair.first = SecureBinaryData(pubKey);
   outKeyPair.second = SecureBinaryData(prvKey);

   return retVal;
}

// Function that reads a file with a Z85-encoded CurveZMQ key. The key will be
// 40 bytes + a null char.
//
// IN:  ZMQ key file path (const QString&)
//      Boolean indicator if key is pub or prv (const bool&)
//      Logger (const std::shared_ptr<spdlog::logger>&)
// OUT: A key buffer that will be initialized (SecureBinaryData&)
// RET: Boolean indicator of the read success.
bool bs::network::readZmqKeyFile(const QString& zmqKeyFilePath
   , SecureBinaryData& zmqKey, const bool& isPub
   , const std::shared_ptr<spdlog::logger>& logger)
{
   const size_t targetFileSize = isPub ? CURVEZMQPUBKEYBUFFERSIZE : CURVEZMQPRVKEYBUFFERSIZE;

   // Read the private key file and make sure it's properly formatted.
   QFile zmqFile(zmqKeyFilePath);
   if (!zmqFile.open(QIODevice::ReadOnly)) {
      if (logger) {
         logger->error("[ZmqSecuredServerConnection::{}] ZMQ key file ({}) "
            "cannot be opened.", __func__, zmqKeyFilePath.toStdString());
      }
      return false;
   }

   if (zmqFile.size() != targetFileSize) {
      if (logger) {
         logger->error("[ZmqSecuredServerConnection::{}] ZMQ key file size "
            "({} bytes) should be {} bytes.", __func__, zmqFile.size()
            , isPub ? CURVEZMQPUBKEYBUFFERSIZE : CURVEZMQPRVKEYBUFFERSIZE);
      }
      return false;
   }

   const auto readBuf = zmqFile.readAll();
   zmqFile.close();
   return readZmqKeyString(readBuf, zmqKey, isPub, logger);
}

bool bs::network::readZmqKeyString(const QByteArray& zmqEncodedKey, SecureBinaryData &zmqKey, const bool &isPub, const std::shared_ptr<spdlog::logger> &logger)
{
   if (zmqEncodedKey.isEmpty()) {
      return false;
   }

   const size_t targetFileSize = isPub ? CURVEZMQPUBKEYBUFFERSIZE : CURVEZMQPRVKEYBUFFERSIZE;
   SecureBinaryData junkBuf(32);

   if (zmq_z85_decode(junkBuf.getPtr(), zmqEncodedKey.toStdString().c_str()) == NULL) {
      if (logger) {
         logger->error("[ZmqSecuredServerConnection::{}] ZMQ key string "
            "is not a Z85-formatted key file.", __func__);
      }
      return false;
   }

   zmqKey = SecureBinaryData(zmqEncodedKey.toStdString());
   if (zmqKey.getSize() != targetFileSize) {
      if (logger) {
         logger->error("[ZmqSecuredServerConnection::{}] ZMQ key string ({}) "
            "is {} bytes, not {} bytes", __func__, zmqKey.getSize()
            , targetFileSize);
      }
      return false;
   }
   return true;
}
