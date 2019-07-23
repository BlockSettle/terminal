#include "StringUtils.h"

#include <botan/hex.h>

namespace bs {

std::string toHex(const std::string &str, bool uppercase)
{
   return Botan::hex_encode(reinterpret_cast<const uint8_t*>(str.data()), str.size(), uppercase);
}

std::string toLower(std::string str)
{
   std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c){
      return std::tolower(c);
   });
   return str;
}

std::string toUpper(std::string str)
{
   std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) {
      return std::toupper(c);
   });
   return str;
}

} // namespace bs
