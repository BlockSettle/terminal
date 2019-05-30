#pragma once

#ifndef IES_DECRYPTION_H
#define IES_DECRYPTION_H

#include "Cipher.h"

namespace Encryption
{

   class IES_Decryption : public Cipher
   {
   public:
      IES_Decryption(const std::shared_ptr<spdlog::logger>&);
      ~IES_Decryption() = default;

      static std::unique_ptr<IES_Decryption> create(const std::shared_ptr<spdlog::logger>& logger);

      void finish(Botan::SecureVector<uint8_t>& data) override;
   };

}

#endif // IES_DECRYPTION_H