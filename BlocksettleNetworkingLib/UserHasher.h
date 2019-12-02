/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __USER_HASHER_H__
#define __USER_HASHER_H__

#include <memory>
#include <string>

class BinaryData;
struct KeyDerivationFunction;

class UserHasher {
public:
   static const int KeyLength;

   UserHasher();
   UserHasher(const BinaryData& iv);

   std::shared_ptr<KeyDerivationFunction> getKDF() const { return kdf_; }

   std::string deriveKey(const std::string& rawData) const;
private:
   std::shared_ptr<KeyDerivationFunction> kdf_;
};

using UserHasherPtr = std::shared_ptr<UserHasher>;

#endif //__USER_HASHER_H__
