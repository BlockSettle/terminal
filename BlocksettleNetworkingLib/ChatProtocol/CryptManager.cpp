/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include <QtConcurrent/QtConcurrent>
#include <QFuture>
#include <utility>

#include "ChatProtocol/CryptManager.h"
#include "Encryption/IES_Encryption.h"
#include "Encryption/IES_Decryption.h"
#include "Encryption/AEAD_Encryption.h"
#include "Encryption/AEAD_Decryption.h"

#include <disable_warnings.h>
#include <spdlog/logger.h>
#include "botan/base64.h"
#include "BinaryData.h"
#include "SecureBinaryData.h"
#include <enable_warnings.h>

using namespace Chat;

CryptManager::CryptManager(LoggerPtr loggerPtr, QObject* parent /* = nullptr */)
   : QObject(parent), loggerPtr_(std::move(loggerPtr))
{

}

std::string CryptManager::validateUtf8(const Botan::SecureVector<uint8_t>& data) const
{
   auto result = QString::fromUtf8(reinterpret_cast<const char*>(data.data()), static_cast<int>(data.size())).toStdString();
   if (result.size() != data.size()) {
      throw std::runtime_error("invalid utf text detected - different size");
   }

   const auto isEqual = std::equal(data.begin(), data.end(), result.begin(), [](uint8_t left, uint8_t right) -> bool {
      return left == right;
   });

   if (!isEqual) {
      throw std::runtime_error("invalid utf text detected - incorrect data");
   }

   return result;
}

QFuture<std::string> CryptManager::encryptMessageIES(const std::string& message, const BinaryData& ownPublicKey)
{
   const auto encryptMessageWorker = [this, ownPublicKey](const std::string& message)
   {
      auto cipher = Encryption::IES_Encryption::create(loggerPtr_);

      cipher->setPublicKey(ownPublicKey);
      cipher->setData(message);

      Botan::SecureVector<uint8_t> output;
      try
      {
         cipher->finish(output);
      }
      catch (const std::exception& e)
      {
         loggerPtr_->error("Can't encrypt message: {}", e.what());
      }

      auto encryptedMessage = base64_encode(output);

      return encryptedMessage;
   };

   return QtConcurrent::run(encryptMessageWorker, message);
}

QFuture<std::string> CryptManager::decryptMessageIES(const std::string& message, const SecureBinaryData& ownPrivateKey)
{
   const auto decryptMessageWorker = [this, ownPrivateKey](const std::string& message)
   {
      auto decipher = Encryption::IES_Decryption::create(loggerPtr_);

      Botan::secure_vector<uint8_t> data;
      try
      {
         data = Botan::base64_decode(message);
      }
      catch (const std::exception& e)
      {
         loggerPtr_->error("Can't decode message {}", e.what());
      }

      decipher->setData(std::string(data.begin(), data.end()));
      decipher->setPrivateKey(ownPrivateKey);

      Botan::SecureVector<uint8_t> output;
      std::string decryptedMessage;

      try
      {
         decipher->finish(output);
         decryptedMessage = validateUtf8(output);
      }
      catch (const std::exception& e)
      {
         loggerPtr_->error("Can't decrypt message: {}", e.what());
      }

      return decryptedMessage;
   };

   return QtConcurrent::run(decryptMessageWorker, message);
}

std::string CryptManager::jsonAssociatedData(const std::string& partyId, const BinaryData& nonce)
{
   QJsonObject data;
   data[QLatin1String("partyId")] = QString::fromStdString(partyId);
   data[QLatin1String("nonce")] = QString::fromStdString(nonce.toHexStr());
   const QJsonDocument jsonDocument(data);
   return jsonDocument.toJson(QJsonDocument::Compact).toStdString();
}

QFuture<std::string> CryptManager::encryptMessageAEAD(const std::string& message, const std::string& associatedData,
   const SecureBinaryData& localPrivateKey, const BinaryData& nonce, const BinaryData& remotePublicKey)
{
   const auto encryptMessageWorker = [this, associatedData, localPrivateKey, nonce, remotePublicKey](const std::string& message)
   {
      auto cipher = Encryption::AEAD_Encryption::create(loggerPtr_);

      cipher->setPrivateKey(localPrivateKey);
      cipher->setPublicKey(remotePublicKey);
      cipher->setNonce(Botan::SecureVector<uint8_t>(nonce.getPtr(), nonce.getPtr() + nonce.getSize()));
      cipher->setData(message);
      loggerPtr_->info("[CryptManager::encryptMessageAEAD] jsonAssociatedData: {}", associatedData);
      cipher->setAssociatedData(associatedData);

      Botan::SecureVector<uint8_t> output;

      try
      {
         cipher->finish(output);
      }
      catch (const std::exception& e)
      {
         loggerPtr_->error("Can't encrypt message: {}", e.what());
      }

      auto encryptedMessage = base64_encode(output);

      return encryptedMessage;
   };

   return QtConcurrent::run(encryptMessageWorker, message);
}

QFuture<std::string> CryptManager::decryptMessageAEAD(const std::string& message, const std::string& associatedData,
   const SecureBinaryData& localPrivateKey, const BinaryData& nonce, const BinaryData& remotePublicKey)
{
   const auto decryptMessageWorker = [this, associatedData, localPrivateKey, nonce, remotePublicKey](const std::string& message)
   {
      auto decipher = Encryption::AEAD_Decryption::create(loggerPtr_);

      Botan::secure_vector<uint8_t> data;
      try
      {
         data = Botan::base64_decode(message);
      }
      catch (const std::exception& e)
      {
         loggerPtr_->error("Can't decode message {}", e.what());
      }

      decipher->setData(std::string(data.begin(), data.end()));
      decipher->setPrivateKey(localPrivateKey);
      decipher->setPublicKey(remotePublicKey);

      const auto& nonceVector = Botan::SecureVector<uint8_t>(nonce.getPtr(), nonce.getPtr() + nonce.getSize());
      decipher->setNonce(nonceVector);

      loggerPtr_->info("[CryptManager::decryptMessageAEAD] jsonAssociatedData: {}", associatedData);
      decipher->setAssociatedData(associatedData);

      Botan::SecureVector<uint8_t> output;
      std::string decryptedMessage;

      try
      {
         decipher->finish(output);
         decryptedMessage = validateUtf8(output);
      }
      catch (const std::exception& e)
      {
         loggerPtr_->error("Can't decrypt message: {}", e.what());
      }

      return decryptedMessage;
   };

   return QtConcurrent::run(decryptMessageWorker, message);
}
