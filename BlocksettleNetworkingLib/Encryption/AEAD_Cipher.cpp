/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "AEAD_Cipher.h"

#include <disable_warnings.h>

#include "BinaryData.h"
#include "SecureBinaryData.h"
#include <botan/ec_group.h>
#include <botan/ecdh.h>
#include <botan/auto_rng.h>
#include <botan/pubkey.h>

#include <enable_warnings.h>

namespace Encryption
{

   AEAD_Cipher::AEAD_Cipher(const std::shared_ptr<spdlog::logger>& logger) : Cipher(logger)
   {

   }

   Botan::SecureVector<uint8_t> AEAD_Cipher::nonce() const
   {
      return nonce_;
   }

   void AEAD_Cipher::setNonce(const Botan::SecureVector<uint8_t>& nonce)
   {
      nonce_ = Botan::SecureVector<uint8_t>(nonce.begin(), nonce.end());
   }

   void AEAD_Cipher::setAssociatedData(const std::string& data)
   {
      associatedData_ = Botan::SecureVector<uint8_t>(data.begin(), data.end());
   }

   Botan::SecureVector<uint8_t> AEAD_Cipher::associatedData() const
   {
      return associatedData_;
   }

   Botan::SymmetricKey AEAD_Cipher::getSymmetricKey() const
   {
      if (publicKey_ == nullptr) {
         throw std::runtime_error("Public key is empty.");
      }

      Botan::EC_Group kDomain(EC_GROUP);
      Botan::PointGFp publicKeyValue = kDomain.OS2ECP(publicKey_->getPtr(), publicKey_->getSize());
      Botan::ECDH_PublicKey publicKeyDecoded(kDomain, publicKeyValue);

      if (privateKey_ == nullptr) {
         throw std::runtime_error("Private key is empty.");
      }

      Botan::AutoSeeded_RNG rng;
      Botan::BigInt privateKeyValue;
      privateKeyValue.binary_decode(privateKey_->getPtr(), privateKey_->getSize());
      Botan::ECDH_PrivateKey privateKeyDecoded(rng, kDomain, privateKeyValue);
      privateKeyValue.clear();

      // Generate symmetric key from public and private key
      Botan::PK_Key_Agreement key_agreement(privateKeyDecoded, rng, KDF2);
      Botan::SymmetricKey symmetricKey = key_agreement.derive_key(SYMMETRIC_KEY_LEN, publicKeyDecoded.public_value()).bits_of();

      return symmetricKey;
   }

}

