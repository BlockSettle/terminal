/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "PublicResolver.h"

bs::PublicResolver::PublicResolver(const std::map<bs::Address, BinaryData> &preimageMap)
   : ResolverFeed()
{
   for (const auto &preimage : preimageMap) {
      preimageMap_[preimage.first.unprefixed()] = preimage.second;
   }
}

bs::PublicResolver::PublicResolver(const std::map<std::string, BinaryData> &preimageMap)
   : ResolverFeed()
{
   for (const auto &preimage : preimageMap) {
      preimageMap_[Address::fromAddressString(preimage.first).unprefixed()] = preimage.second;
   }
}

BinaryData bs::PublicResolver::getByVal(const BinaryData &addr)
{
   const auto itAddr = preimageMap_.find(addr);
   if (itAddr != preimageMap_.end()) {
      return itAddr->second;
   }
   throw std::runtime_error("not found");
}

const SecureBinaryData& bs::PublicResolver::getPrivKeyForPubkey(const BinaryData &pk)
{
   throw std::runtime_error("not supported");
}
