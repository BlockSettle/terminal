/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __EASY_CODEC_H__
#define __EASY_CODEC_H__

#include <string>
#include <unordered_map>
#include <unordered_set>

class EasyCoDec
{
public:
   EasyCoDec();

   struct Data {
      std::string part1;
      std::string part2;
   };

   Data fromHex(const std::string &) const;
   std::string fromHexToString(const std::string &) const;

   std::string toHex(const Data &) const;
   std::string toHex(const std::string &in) const;

   const std::unordered_set<char> &allowedChars() const { return allowedChars_; }

private:
   const std::unordered_map<char, char>   fromHex_;
   std::unordered_map<char, char>         toHex_;
   std::unordered_set<char>               allowedChars_;
};


#endif	//__EASY_CODEC_H__
