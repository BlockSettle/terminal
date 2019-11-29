/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "WalletEncryption.h"

//no

/*
BinaryData mergeKeys(const BinaryData &a, const BinaryData &b)
{
   BinaryData result;
   if (a.getSize() > b.getSize()) {
      result = a;
      for (size_t i = 0; i < b.getSize(); ++i) {
         *(result.getPtr() + i) ^= *(b.getPtr() + i);
      }
   }
   else {
      result = b;
      for (size_t i = 0; i < a.getSize(); ++i) {
         *(result.getPtr() + i) ^= *(a.getPtr() + i);
      }
   }
   return result;
}
*/
#include "moc_WalletEncryption.cpp"
