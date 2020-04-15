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

Path getDerivationPath(bool testNet, Purpose element)
{
   Path path;
   path.append(hardFlag | element);
   path.append(testNet ? CoinType::Bitcoin_test : CoinType::Bitcoin_main);
   path.append(hardFlag);
   return path;
}
