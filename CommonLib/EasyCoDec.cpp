/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include <exception>
#include "EasyCoDec.h"

#ifdef _WIN32
#if (_MSC_VER >= 1920)
#include <stdexcept>
#endif
#endif

EasyCoDec::EasyCoDec() : fromHex_({
   { '0', 'a' },
   { '1', 's' },
   { '2', 'd' },
   { '3', 'f' },
   { '4', 'g' },
   { '5', 'h' },
   { '6', 'j' },
   { '7', 'k' },
   { '8', 'w' },
   { '9', 'e' },
   { 'a', 'r' },
   { 'b', 't' },
   { 'c', 'u' },
   { 'd', 'i' },
   { 'e', 'o' },
   { 'f', 'n' } })
{
   for (const auto &elem : fromHex_) {
      toHex_[elem.second] = elem.first;
      allowedChars_.insert(elem.second);
   }
}

std::string EasyCoDec::fromHexToString(const std::string &s) const
{
   const auto len = s.length();
   std::string result;
   result.reserve(len + len / 4);

   for (size_t i = 0; i < len; i++) {
      const auto it = fromHex_.find(::tolower(s[i]));
      if (it != fromHex_.end()) {
         result.push_back(it->second);
         if ((i % 4 == 3) && (i < (len - 1))) {
            result.push_back(' ');
         }
      }
      else {
         throw std::invalid_argument("invalid hex data at " + std::to_string(i));
      }
   }
   return result;
}

EasyCoDec::Data EasyCoDec::fromHex(const std::string &s) const
{
   if ((s.length() % 4) != 0) {
      throw std::invalid_argument("invalid hex string length: " + std::to_string(s.length()) + " (should divide by 4)");
   }
   const auto halfLen = s.length() / 2;
   const auto &substr1 = s.substr(0, halfLen);
   const auto &substr2 = s.substr(halfLen, halfLen);
   Data data;
   data.part1 = fromHexToString(substr1);
   data.part2 = fromHexToString(substr2);
/*   size_t halfLen = s.length() / 2;
   data.part1.reserve(halfLen + halfLen / 4);
   data.part2.reserve(halfLen + halfLen / 4);

   for (size_t i = 0; i < halfLen; i++) {
      const auto it = fromHex_.find(::tolower(s[i]));
      if (it != fromHex_.end()) {
         data.part1.push_back(it->second);
         if ((i % 4 == 3) && (i < (halfLen - 1))) {
            data.part1.push_back(' ');
         }
      }
      else {
         throw std::invalid_argument("invalid hex data at " + std::to_string(i));
      }
   }

   for (size_t i = halfLen; i < s.length(); i++) {
      const auto it = fromHex_.find(::tolower(s[i]));
      if (it != fromHex_.end()) {
         data.part2.push_back(it->second);
         if ((i % 4 == 3) && (i < (s.length() - 1))) {
            data.part2.push_back(' ');
         }
      }
      else {
         throw std::invalid_argument("invalid hex data at " + std::to_string(i));
      }
   }*/

   return data;
}

std::string EasyCoDec::toHex(const EasyCoDec::Data &data) const
{
   if ((data.part1.length() != data.part2.length()) || data.part1.empty()) {
      throw std::invalid_argument("invalid part lengths: " + std::to_string(data.part1.length())
         + ", " + std::to_string(data.part2.length()));
   }
   const auto hexHalf1 = toHex(data.part1);
   const auto hexHalf2 = toHex(data.part2);

   if (hexHalf1.length() != hexHalf2.length()) {
      throw std::invalid_argument("decoded halves are not equal");
   }
   if (hexHalf1.empty()) {
      throw std::invalid_argument("failed to decode");
   }
   return (hexHalf1 + hexHalf2);
}

std::string EasyCoDec::toHex(const std::string &in) const
{
   std::string result;
   result.reserve(in.length());
   for (size_t i = 0; i < in.length(); i++) {
      if (in[i] == ' ') {
         continue;
      }
      const auto it = toHex_.find(::tolower(in[i]));
      if (it == toHex_.end()) {
         throw std::invalid_argument("invalid easy data at " + std::to_string(i));
      }
      result.push_back(it->second);
   }
   return result;
};
