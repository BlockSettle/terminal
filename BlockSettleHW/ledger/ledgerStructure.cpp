/*

***********************************************************************************
* Copyright (C) 2016 - , BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "ledger/ledgerStructure.h"

void writeVarInt(QByteArray &output, size_t size) {

   if (size < 0xfd) {
      output.push_back(static_cast<unsigned char>(size));
   }
   else if (size <= 0xffff) {
      output.push_back(static_cast<unsigned char>(0xfd));
      writeUintLE(output, static_cast<uint16_t>(size & 0xffff));
   }
   else if (size <= 0xffffffff) {
      output.push_back(static_cast<unsigned char>(0xfe));
      writeUintLE(output, static_cast<uint32_t>(size & 0xffffffff));
   }
}
