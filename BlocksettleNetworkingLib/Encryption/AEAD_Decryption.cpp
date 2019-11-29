/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "AEAD_Decryption.h"

#include <disable_warnings.h>
#include <spdlog/spdlog.h>

#include <botan/aead.h>
#include <enable_warnings.h>

namespace Encryption
{

   AEAD_Decryption::AEAD_Decryption(const std::shared_ptr<spdlog::logger>& logger) : AEAD_Cipher(logger)
   {
   }

   std::unique_ptr<AEAD_Decryption> AEAD_Decryption::create(const std::shared_ptr<spdlog::logger>& logger)
   {
      return std::unique_ptr<AEAD_Decryption>(new AEAD_Decryption(logger));
   }

   void AEAD_Decryption::finish(Botan::SecureVector<uint8_t>& data)
   {
      Botan::SymmetricKey symmetricKey;
      try {
          symmetricKey = getSymmetricKey();
      }
      catch (std::exception& e) {
         logger_->error("[AEAD_Decryption::{}] Invalid symmetric key{}", __func__, e.what());
      }

      std::unique_ptr<Botan::AEAD_Mode> decryptor = Botan::AEAD_Mode::create(AEAD_ALGO, Botan::DECRYPTION);

      try {
         decryptor->set_key(symmetricKey);
      }
      catch (Botan::Exception & e) {
         logger_->error("[AEAD_Decryption::{}] Invalid symmetric key {}", __func__, e.what());
         return;
      }

      if (!associatedData().empty()) {
         decryptor->set_ad(associatedData());
      }

      if (nonce().empty()) {
         throw std::runtime_error("Nonce is empty.");
      }

      try {
         decryptor->start(nonce());
      }
      catch (Botan::Exception & e) {
         logger_->error("[AEAD_Decryption::{}] Invalid nonce {}", __func__, e.what());
         return;
      }

      Botan::SecureVector<uint8_t> decrypted_data = data_;

      if (decrypted_data.size() < decryptor->minimum_final_size())
      {
         logger_->error("[AEAD_Decryption::{}] Decryption data size ({}) is less than the anticipated size ({})", __func__, decrypted_data.size(), decryptor->minimum_final_size());
         return;
      }

      try {
         decryptor->finish(decrypted_data);
      }
      catch (Botan::Exception & e) {
         logger_->debug("[AEAD_Decryption::{}] Decryption message failed {}", __func__, e.what());
         return;
      }

      data = decrypted_data;
   }

}
