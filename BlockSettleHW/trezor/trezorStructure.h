/*

***********************************************************************************
* Copyright (C) 2020 - 2021, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef TREZORSTRUCTURE_H
#define TREZORSTRUCTURE_H

#include <string>

namespace bs {
   namespace hww {
      namespace trezor {

         struct DeviceData
         {
            std::string path;
            int         vendor;
            int         product;
            std::string sessionId;
            bool        debug{ false };
            std::string debugSession;
         };

         enum class State {
            None = 0,
            Init,
            Enumerated,
            Acquired,
            Released
         };

         struct MessageData
         {
            int type = -1;
            int length = -1;
            std::string message;
         };

         enum class InfoStatus {
            Unknown,
            RequestPassphrase,
            RequestPIN
         };

      }  //trezor
   }  //hw
}     //bs

//namespace HWInfoStatus {
   //const QString kRequestPassphrase = QObject::tr("Please enter the trezor passphrase");
   //const QString kRequestPin = QObject::tr("Please enter the pin from device");
//}

#endif // TREZORSTRUCTURE_H
