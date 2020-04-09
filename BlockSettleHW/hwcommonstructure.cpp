/*

***********************************************************************************
* Copyright (C) 2016 - , BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/

#include "hwcommonstructure.h"
using namespace bs::hd;

Path getDerivationPath(bool testNet, bool isNestedSegwit)
{
   Path path;
   if (isNestedSegwit) {
      path.append(Purpose::Nested);
   }
   else {
      path.append(Purpose::Native);
   }

   if (testNet) {
      path.append(hardFlag | 1);
   }
   else {
      path.append(hardFlag);
   }

   path.append(hardFlag);

   return path;
}

