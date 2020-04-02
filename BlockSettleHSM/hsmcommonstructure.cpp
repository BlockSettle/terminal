/*

***********************************************************************************
* Copyright (C) 2016 - , BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/

#include "hsmcommonstructure.h"

std::vector<uint32_t> getDerivationPath(bool testNet, bool isNestedSegwit)
{
   std::vector<uint32_t> path;
   if (isNestedSegwit) {
      path.push_back(0x80000031);
   }
   else {
      path.push_back(0x80000054);
   }

   if (testNet) {
      path.push_back(0x80000001);
   }
   else {
      path.push_back(0x80000000);
   }

   path.push_back(0x80000000);

   return path;
}

