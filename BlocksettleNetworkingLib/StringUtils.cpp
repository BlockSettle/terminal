#include "StringUtils.h"

#include <botan/hex.h>
#include <locale>

namespace bs {

std::string toHex(const std::string &str, bool uppercase)
{
   return Botan::hex_encode(reinterpret_cast<const uint8_t*>(str.data()), str.size(), uppercase);
}

std::string toLower(std::string str)
{
#ifdef __APPLE__
   std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) {
      return std::tolower(c);
   });
   return str;
#else
   std::locale locale("C");
   std::transform(str.begin(), str.end(), str.begin(), [&locale](unsigned char c) {
      return std::tolower(c, locale);
   });
   return str;
#endif
}

std::string toUpper(std::string str)
{
#ifdef __APPLE__
   std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) {
      return std::toupper(c);
   });
   return str;
#else
   std::locale locale("C");
   std::transform(str.begin(), str.end(), str.begin(), [&locale](unsigned char c) {
      return std::toupper(c, locale);
   });
   return str;
#endif
}

} // namespace bs
