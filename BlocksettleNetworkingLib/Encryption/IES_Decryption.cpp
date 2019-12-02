/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "IES_Decryption.h"

#include <disable_warnings.h>

#include "BinaryData.h"
#include "SecureBinaryData.h"

#include <spdlog/spdlog.h>

#include <botan/ec_group.h>
#include <botan/ecdh.h>
#include <botan/auto_rng.h>
#include <botan/ecies.h>
#include <botan/secmem.h>
#include <botan/bigint.h>

#include <enable_warnings.h>

namespace Encryption
{

   IES_Decryption::IES_Decryption(const std::shared_ptr<spdlog::logger>& logger) : Cipher(logger)
   {
   }

   std::unique_ptr<IES_Decryption> IES_Decryption::create(const std::shared_ptr<spdlog::logger>& logger)
   {
      return std::unique_ptr<IES_Decryption>(new IES_Decryption(logger));
   }

   void IES_Decryption::finish(Botan::SecureVector<uint8_t>& data)
   {
      if (privateKey_ == nullptr) {
         throw std::runtime_error("Private key is empty.");
      }

      try {
         Botan::AutoSeeded_RNG rng;

         Botan::EC_Group kDomain(EC_GROUP);
         const Botan::ECIES_System_Params kEciesParams(kDomain,
            KDF2, IES_ALGO, IES_KEY_LEN, IES_MAC_ALGO, IES_MAC_LEN,
            Botan::PointGFp::COMPRESSED, Botan::ECIES_Flags::NONE);

         Botan::BigInt privateKeyValue;
         privateKeyValue.binary_decode(privateKey_->getPtr(), privateKey_->getSize());
         Botan::ECDH_PrivateKey privateKeyDecoded(rng, kDomain, privateKeyValue);
         privateKeyValue.clear();

         Botan::ECIES_Decryptor decryptor(privateKeyDecoded, kEciesParams, rng);

         auto output = decryptor.decrypt(data_);
         data = Botan::SecureVector<uint8_t>(output.begin(), output.end());
      }
      catch (Botan::Exception & e) {
         logger_->error("[IES_Decryption::{}] Decryption error {}", __func__, e.what());
         return;
      }
   }

}

