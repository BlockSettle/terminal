#include "LoginHasher.h"

#include <cassert>

#include "BinaryData.h"
#include "StringUtils.h"

namespace {
   const std::string DefaultIvWithoutSalt =
      "2b"                //total_length
      "00c1"              //Romix_KDF
      "01000000"          //Iterations
      "00040000"          //memory_target
      "20";               //salt_length
}

LoginHasher::LoginHasher(const BinaryData &salt)
   : hasher_(BinaryData::CreateFromHex(DefaultIvWithoutSalt) + salt)
{
   assert(salt.getSize() == 32);
}

std::string LoginHasher::hashLogin(const std::string &login) const
{
   return hasher_.deriveKey(bs::toLower(login));
}
