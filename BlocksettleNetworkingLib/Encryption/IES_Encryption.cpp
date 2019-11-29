/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "IES_Encryption.h"

#include <disable_warnings.h>

#include "BinaryData.h"

#include <spdlog/spdlog.h>

#include <botan/ec_group.h>
#include <botan/ecdh.h>
#include <botan/auto_rng.h>
#include <botan/ecies.h>
#include <botan/secmem.h>

#include <enable_warnings.h>

namespace Encryption
{

   IES_Encryption::IES_Encryption(const std::shared_ptr<spdlog::logger>& logger) : Cipher(logger)
   {
   }

   std::unique_ptr<IES_Encryption> IES_Encryption::create(const std::shared_ptr<spdlog::logger>& logger)
   {
      return std::unique_ptr<IES_Encryption>(new IES_Encryption(logger));
   }

   void IES_Encryption::finish(Botan::SecureVector<uint8_t>& data)
   {
      if (publicKey_ == nullptr) {
         throw std::runtime_error("Public key is empty.");
      }

      Botan::AutoSeeded_RNG rng;

      Botan::EC_Group kDomain(EC_GROUP);
      const Botan::ECIES_System_Params kEciesParams(kDomain,
         KDF2, IES_ALGO, IES_KEY_LEN, IES_MAC_ALGO, IES_MAC_LEN,
         Botan::PointGFp::COMPRESSED, Botan::ECIES_Flags::NONE);

      try {
         Botan::ECIES_Encryptor encrypt(rng, kEciesParams);

         Botan::PointGFp publicKeyValue = kDomain.OS2ECP(publicKey_->getPtr(), publicKey_->getSize());

         encrypt.set_other_key(publicKeyValue);

         auto output = encrypt.encrypt(data_, rng);
         data = Botan::SecureVector<uint8_t>(output.begin(), output.end());
      }
      catch (Botan::Exception & e) {
         logger_->error("[IES_Encryption::{}] Encryption error {}", __func__, e.what());
         return;
      }
   }

}
