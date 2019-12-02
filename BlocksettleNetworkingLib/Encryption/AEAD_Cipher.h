/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#pragma once

#ifndef AEAD_CIPHER_H
#define AEAD_CIPHER_H

#include "Encryption/Cipher.h"

#include <disable_warnings.h>
#include <botan/pubkey.h>
#include <enable_warnings.h>

namespace Encryption
{
   const size_t SYMMETRIC_KEY_LEN = 32;
   const std::string AEAD_ALGO = "ChaCha20Poly1305";

   class AEAD_Cipher : public Cipher
   {
   public:
      AEAD_Cipher(const std::shared_ptr<spdlog::logger>&);
      ~AEAD_Cipher() = default;

      Botan::SecureVector<uint8_t> nonce() const;
      void setNonce(const Botan::SecureVector<uint8_t>&);

      void setAssociatedData(const std::string& data);

   protected:
      Botan::SecureVector<uint8_t> associatedData() const;
      Botan::SymmetricKey getSymmetricKey() const;

   private:
      Botan::SecureVector<uint8_t> nonce_;
      Botan::SecureVector<uint8_t> associatedData_;
   };
}

#endif // AEAD_CIPHER_H