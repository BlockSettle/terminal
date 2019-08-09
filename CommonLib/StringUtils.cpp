#include "StringUtils.h"

#include <algorithm>
#include <cctype>

namespace {

   const char HexUpper[16] = {
      '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
      'A', 'B', 'C', 'D', 'E', 'F' };

   const char HexLower[16] = {
      '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
      'a', 'b', 'c', 'd', 'e', 'f' };

} // namespace

namespace bs {

   // Copied from Botan
   std::string toHex(const std::string &str, bool uppercase)
   {
      const char* tbl = uppercase ? HexUpper : HexLower;

      std::string result;
      result.reserve(str.length() * 2);
      for (size_t i = 0; i < str.length(); ++i) {
         auto x = uint8_t(str[i]);
         result.push_back(tbl[(x >> 4) & 0x0F]);
         result.push_back(tbl[(x) & 0x0F]);
      }

      return result;
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
