/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
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

      // first - required number of keys (M), second - total number of keys (N)
      // now supporting only 1-of-N, and KeyRank is not used
      using KeyRank = struct {
         unsigned int   m;
         unsigned int   n;
      };

      struct PasswordMetaData {
         EncryptionType    encType;
         BinaryData        encKey;
      };

      struct PasswordData {
         SecureBinaryData  password;
         PasswordMetaData  metaData;
         BinaryData        salt;
         SecureBinaryData  controlPassword;
      };
   }  // wallet
}  //namespace bs

//BinaryData mergeKeys(const BinaryData &, const BinaryData &);

#endif //WALLET_ENCRYPTION_H
