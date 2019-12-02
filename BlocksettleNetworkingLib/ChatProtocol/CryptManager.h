/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef CRYPTMANAGER_H
#define CRYPTMANAGER_H

#include <QObject>
#include <QFuture>
#include <memory>

#include "ChatProtocol/SessionKeyData.h"

namespace spdlog
{
   class logger;
}

class BinaryData;

namespace Chat
{
   using LoggerPtr = std::shared_ptr<spdlog::logger>;

   class CryptManager : public QObject
   {
      Q_OBJECT
   public:
      CryptManager(Chat::LoggerPtr loggerPtr, QObject* parent = nullptr);

      QFuture<std::string> encryptMessageIES(const std::string& message, const BinaryData& ownPublicKey);
      QFuture<std::string> decryptMessageIES(const std::string& message, const SecureBinaryData& ownPrivateKey);

      QFuture<std::string> encryptMessageAEAD(const std::string& message, const std::string& associatedData, 
         const SecureBinaryData& localPrivateKey, const BinaryData& nonce, const BinaryData& remotePublicKey);
      QFuture<std::string> decryptMessageAEAD(const std::string& message, const std::string& associatedData, 
         const SecureBinaryData& localPrivateKey, const BinaryData& nonce, const BinaryData& remotePublicKey);

      static std::string jsonAssociatedData(const std::string& partyId, const BinaryData& nonce);

   private:
      std::string validateUtf8(const Botan::SecureVector<uint8_t>& data) const;

      LoggerPtr loggerPtr_;
   };

   using CryptManagerPtr = std::shared_ptr<CryptManager>;
}

Q_DECLARE_METATYPE(Chat::CryptManagerPtr)

#endif // CRYPTMANAGER_H
