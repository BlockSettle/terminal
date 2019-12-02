/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "AEAD_Encryption.h"

#include <disable_warnings.h>
#include "BinaryData.h"
#include "SecureBinaryData.h"

#include <spdlog/spdlog.h>

#include <botan/ec_group.h>
#include <botan/ecdh.h>
#include <botan/auto_rng.h>
#include <botan/pubkey.h>
#include <botan/aead.h>
#include <enable_warnings.h>

namespace Encryption
{

   AEAD_Encryption::AEAD_Encryption(const std::shared_ptr<spdlog::logger>& logger) : AEAD_Cipher(logger)
   {
   }

   std::unique_ptr<AEAD_Encryption> AEAD_Encryption::create(const std::shared_ptr<spdlog::logger>& logger)
   {
      return std::unique_ptr<AEAD_Encryption>(new AEAD_Encryption(logger));
   }
   
   void AEAD_Encryption::finish(Botan::SecureVector<uint8_t>& data)
   {
      Botan::SymmetricKey symmetricKey;
      try {
         symmetricKey = getSymmetricKey();
      }
      catch (std::exception & e) {
         logger_->error("[AEAD_Encryption::{}] Invalid symmetric key{}", __func__, e.what());
      }

      std::unique_ptr<Botan::AEAD_Mode> encryptor = Botan::AEAD_Mode::create(AEAD_ALGO, Botan::ENCRYPTION);

      try {
         encryptor->set_key(symmetricKey);
      }
      catch (Botan::Exception& e) {
         logger_->error("[AEAD_Encryption::{}] Invalid symmetric key {}", __func__, e.what());
         return;
      }

      if (!associatedData().empty()) {
         try {
            encryptor->set_ad(associatedData());
         }
         catch (Botan::Exception & e) {
            logger_->error("[AEAD_Encryption::{}] Associated data error: {}", __func__, e.what());
         }
      }

      if (nonce().empty()) {
         throw std::runtime_error("Nonce is empty.");
      }

      try {
         encryptor->start(nonce());
      }
      catch (Botan::Exception & e) {
         logger_->error("[AEAD_Encryption::{}] Invalid nonce {}", __func__, e.what());
         return;
      }

      Botan::SecureVector<uint8_t> encrypted_data = data_;

      try {
         encryptor->finish(encrypted_data);
      }
      catch (Botan::Exception & e) {
         logger_->debug("[AEAD_Encryption::{}] Encryption message failed {}", __func__, e.what());
         return;
      }

      data = encrypted_data;
   }

}

