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
         std::shared_ptr<Worker> worker(const std::shared_ptr<InData>&) override final;

      private:
         std::shared_ptr<spdlog::logger>  logger_;
         DeviceCallbacks* cb_{ nullptr };
         const bool  testNet_;
      };


      struct JadeIn : public bs::InData
      {
         JadeIn(const std::string& s, const nlohmann::json& req)
            : serial(s), request(req) {}
         ~JadeIn() override = default;
         const std::string    serial;
         const nlohmann::json request;
      };
      struct JadeOut : public bs::OutData
      {
         JadeOut(const std::string& s) : serial(s) {}
         ~JadeOut() override = default;
         const std::string serial;
         nlohmann::json    response;
         std::string       error;
      };

      class JadeHandler : public bs::HandlerImpl<JadeIn, JadeOut>
      {
      public:
         JadeHandler(const std::shared_ptr<spdlog::logger>&);
         ~JadeHandler() override = default;

      protected:
         std::shared_ptr<JadeOut> processData(const std::shared_ptr<JadeIn>&) override;

      private:
         std::shared_ptr<spdlog::logger>  logger_;
         uint32_t seqNo_{ 0 };
      };

   }  //hw
}     //bs

#endif // JADE_CLIENT_H
