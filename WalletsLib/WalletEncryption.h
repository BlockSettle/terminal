#ifndef WALLET_ENCRYPTION_H
#define WALLET_ENCRYPTION_H

#include "BinaryData.h"
#include "EncryptionUtils.h"


namespace bs {
   namespace wallet {
      enum class EncryptionType : uint8_t {
         Unencrypted,
         Password,
         Freja
      };

      using KeyRank = std::pair<unsigned int, unsigned int>;

      struct PasswordData {
         SecureBinaryData  password;
         EncryptionType    encType;
         SecureBinaryData  encKey;
      };
   }  // wallet
}  //namespace bs

BinaryData xor(const BinaryData &, const BinaryData &);

#endif //WALLET_ENCRYPTION_H
