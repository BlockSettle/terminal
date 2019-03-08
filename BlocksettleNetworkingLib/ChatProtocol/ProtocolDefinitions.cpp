#include "ProtocolDefinitions.h"

#include <memory>

#include <QJsonValue>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace Chat
{
   autheid::PublicKey publicKeyFromString(const std::string &s)
   {
      QByteArray copy(s.data());
      QByteArray value = QByteArray::fromBase64(copy);
      autheid::PublicKey key(value.begin(), value.end());
      return key;
   }
   
   std::string publicKeyToString(const autheid::PublicKey &k)
   {
      QByteArray copy(reinterpret_cast<const char*>(k.data()), int(k.size()));
      QByteArray value = copy.toBase64();
      return value.toStdString();
   }
   
} //namespace Chat
