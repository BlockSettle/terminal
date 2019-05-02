#ifndef WALLET_ENCRYPTION_H
#define WALLET_ENCRYPTION_H

//!#include <QObject>

#include "BinaryData.h"
#include "EncryptionUtils.h"


namespace bs {
   namespace wallet {

      enum EncryptionType : uint8_t {
         Unencrypted,
         Password,
         Auth
      };

      //! first - required number of keys, second - total number of keys
      using KeyRank = std::pair<unsigned int, unsigned int>;

      struct PasswordData {
         SecureBinaryData  password;
         EncryptionType    encType;
         SecureBinaryData  encKey;
      };
   }  // wallet
}  //namespace bs

//BinaryData mergeKeys(const BinaryData &, const BinaryData &);

#endif //WALLET_ENCRYPTION_H
