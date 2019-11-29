/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#pragma once

#ifndef CIPHER_H
#define CIPHER_H

#include <memory>
#include <string>

#include <disable_warnings.h>
#include <botan/secmem.h>
#include <enable_warnings.h>

class BinaryData;
class SecureBinaryData;

namespace spdlog
{
   class logger;
}

namespace Encryption
{
   enum AlgoType : int { AEAD, IES };
   enum CipherDirection : int { ENCRYPTION, DECRYPTION };

   const std::string EC_GROUP = "secp256k1";
   const std::string KDF2 = "KDF2(SHA-256)";
   const std::string IES_ALGO = "ChaCha(20)";
   const int IES_KEY_LEN = 32;
   const std::string IES_MAC_ALGO = "HMAC(SHA-256)";
   const int IES_MAC_LEN = 20;

   class Cipher
   {
   public:
      Cipher(const std::shared_ptr<spdlog::logger>& logger);
      virtual ~Cipher() = default;

      Cipher(const Cipher& c) = delete;
      Cipher& operator=(const Cipher& c) = delete;

      Cipher(Cipher&& c) = delete;
      Cipher& operator=(Cipher&& c) = delete;

      static std::unique_ptr<Cipher> create(const AlgoType& algo, const CipherDirection& cipher_direction, const std::shared_ptr<spdlog::logger>& logger);

      void setPublicKey(const BinaryData& publicKey);
      void setPrivateKey(const SecureBinaryData& privateKey);
      void setData(const std::string& data);

      virtual void finish(Botan::SecureVector<uint8_t>& data) = 0;

   protected:
      std::shared_ptr<BinaryData> publicKey_ = nullptr;
      std::shared_ptr<SecureBinaryData> privateKey_ = nullptr;
      std::shared_ptr<spdlog::logger> logger_ = nullptr;
      Botan::SecureVector<uint8_t> data_;
   };
}

#endif // CIPHER_H