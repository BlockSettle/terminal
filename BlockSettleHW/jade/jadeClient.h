/*

***********************************************************************************
* Copyright (C) 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef JADE_CLIENT_H
#define JADE_CLIENT_H

#include <memory>
#include <nlohmann/json.hpp>
#include "hwdeviceinterface.h"
#include "Message/Worker.h"

namespace spdlog {
   class logger;
}
struct curl_slist;

namespace bs {
   namespace hww {
      class JadeDevice;
      class DeviceCallbacks;

      class JadeClient : protected bs::WorkerPool
      {
         friend class JadeDevice;
      public:
         JadeClient(const std::shared_ptr<spdlog::logger>&
            , bool testNet, DeviceCallbacks*);
         ~JadeClient() override = default;

         void initConnection();
         void scanDevices();

         std::vector<DeviceKey> deviceKeys() const;
         std::shared_ptr<JadeDevice> getDevice(const std::string& deviceId);

      protected:
         //std::shared_ptr<Worker> worker(const std::shared_ptr<InData>&) override final;

      private:
         std::shared_ptr<spdlog::logger>  logger_;
         DeviceCallbacks* cb_{ nullptr };
         const bool  testNet_;
      };

   }  //hw
}     //bs

#endif // JADE_CLIENT_H
