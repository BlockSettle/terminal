/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "Cipher.h"

#include <disable_warnings.h>
#include "BinaryData.h"
#include "SecureBinaryData.h"
#include <enable_warnings.h>

#include "AEAD_Encryption.h"
#include "AEAD_Decryption.h"
#include "IES_Encryption.h"
#include "IES_Decryption.h"

namespace Encryption
{
   Cipher::Cipher(const std::shared_ptr<spdlog::logger>& logger) : logger_(logger)
   {
   }

   std::unique_ptr<Cipher> Encryption::Cipher::create(const AlgoType& algo, const CipherDirection& cipher_direction, const std::shared_ptr<spdlog::logger>& logger)
   {
      if (algo == AEAD) {
         if (cipher_direction == ENCRYPTION) {
            if (auto aead = AEAD_Encryption::create(logger)) {
               return std::unique_ptr<Cipher>(aead.release());
            }
         }
         else {
            if (auto aead = AEAD_Decryption::create(logger)) {
               return std::unique_ptr<Cipher>(aead.release());
            }
         }
      }
      else {
         if (cipher_direction == ENCRYPTION) {
            if (auto ies = IES_Encryption::create(logger)) {
               return std::unique_ptr<Cipher>(ies.release());
            }
         }
         else {
            if (auto ies = IES_Decryption::create(logger)) {
               return std::unique_ptr<Cipher>(ies.release());
            }
         }
      }

      return std::unique_ptr<Cipher>();
   }

   void Cipher::setPublicKey(const BinaryData& publicKey)
   {
      publicKey_ = std::make_shared<BinaryData>(publicKey);
   }

   void Encryption::Cipher::setPrivateKey(const SecureBinaryData& privateKey)
   {
      privateKey_ = std::make_shared<SecureBinaryData>(privateKey);
   }

   void Cipher::setData(const std::string& data)
   {
      data_ = Botan::SecureVector<uint8_t>(data.begin(), data.end());
   }
}