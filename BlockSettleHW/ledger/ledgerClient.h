/*

***********************************************************************************
* Copyright (C) 2020 - 2021, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef LEDGERCLIENT_H
#define LEDGERCLIENT_H

#include <memory>
#include <mutex>
#include <vector>
#include "hwdeviceinterface.h"

namespace spdlog {
   class logger;
}
namespace bs {
   namespace sync {
      class WalletsManager;
   }
}

namespace bs {
   namespace hww {
      struct DeviceCallbacks;
      class DeviceInterface;

      class LedgerClient
      {
      public:
         LedgerClient(const std::shared_ptr<spdlog::logger>&, bool testNet, DeviceCallbacks*);
         ~LedgerClient() = default;

         void scanDevices();

         std::vector<DeviceKey> deviceKeys() const;

         std::shared_ptr<DeviceInterface> getDevice(const std::string& deviceId);

         std::string lastScanError() const;

      private:
         std::shared_ptr<spdlog::logger>  logger_;
         const bool testNet_;
         DeviceCallbacks* cb_{ nullptr };
         std::vector<std::shared_ptr<DeviceInterface>>   availableDevices_;
         std::string lastScanError_;
         std::shared_ptr<std::mutex>   hidLock_;
      };

   }  //hw
}     //bs

#endif // LEDGERCLIENT_H
