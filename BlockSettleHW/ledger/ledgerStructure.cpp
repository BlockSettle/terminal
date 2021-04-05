/*

***********************************************************************************
* Copyright (C) 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "ledger/ledgerStructure.h"
#include "hidapi/hidapi.h"

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

bool checkLedgerDevice(hid_device_info* info) {
   if (!info) {
      return false;
   }

   const bool nanoS = info->vendor_id == Ledger::HID_VENDOR_ID_LEDGER_NANO_S &&
      (info->interface_number == Ledger::HID_INTERFACE_NUMBER
         || info->usage_page == Ledger::HID_USAGE_PAGE);

   const bool nanoX = (info->vendor_id == Ledger::HID_VENDOR_ID_LEDGER_NANO_X) &&
      (Ledger::HID_PRODUCT_ID_LEDGER_NANO_X.count(info->product_id) > 0);

   return nanoS ^ nanoX;
}
