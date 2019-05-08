#pragma once

#ifndef AEAD_DECRYPTION_H
#define AEAD_DECRYPTION_H

#include "AEAD_Cipher.h"

namespace Encryption
{

   class AEAD_Decryption : public AEAD_Cipher
   {
   public:
      AEAD_Decryption(const std::shared_ptr<spdlog::logger>&);
      ~AEAD_Decryption() = default;

      static std::unique_ptr<AEAD_Decryption> create(const std::shared_ptr<spdlog::logger>& logger);

      void finish(Botan::SecureVector<uint8_t>& data) override;
   };

}

#endif // AEAD_DECRYPTION_H