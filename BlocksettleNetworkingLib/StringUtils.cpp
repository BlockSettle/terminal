#include "StringUtils.h"

#include <botan/hex.h>
#include <cctype>

namespace bs {

std::string toHex(const std::string &str, bool uppercase)
{
   return Botan::hex_encode(reinterpret_cast<const uint8_t*>(str.data()), str.size(), uppercase);
}

std::string toLower(std::string str)
{
   std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) {
      return ::tolower(c);
   });
   return str;
}

std::string toUpper(std::string str)
{
   std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) {
      return ::toupper(c);
   });
   return str;
}

} // namespace bs
