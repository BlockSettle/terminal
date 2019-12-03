/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "UserHasher.h"

#include "AssetEncryption.h"
#include "BinaryData.h"
#include "ZBase32.h"

namespace {
   const std::string kdf_iv = "2f"                //total_length
                              "01000000"          //version
                              "01000000"          //Iterations
                              "00040000"          //memory_target
                              "20"                //salt_length
                              "0000000000000000"  //salt
                              "0000000000000000"  //salt+8
                              "0000000000000000"  //salt+16
                              "0000000000000000"; //salt+24
}

const int UserHasher::KeyLength = 12;

UserHasher::UserHasher()
{
   kdf_ = KeyDerivationFunction::deserialize(BinaryData::CreateFromHex(kdf_iv));
}

UserHasher::UserHasher(const BinaryData& iv)
{
   kdf_ = KeyDerivationFunction::deserialize(iv);
}

std::string UserHasher::deriveKey(const std::string& rawData) const
{
   auto key = getKDF()->deriveKey(rawData);
   std::vector<uint8_t> keyData(key.getPtr(), key.getPtr() + key.getSize());
   return bs::zbase32Encode(keyData).substr(0, KeyLength);
}
