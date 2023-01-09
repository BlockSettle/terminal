/*

***********************************************************************************
* Copyright (C) 2020 - 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef TREZORCLIENT_H
#define TREZORCLIENT_H

#include <memory>
#include <nlohmann/json.hpp>
#include "hwdeviceinterface.h"
#include "Message/Worker.h"
#include "trezorStructure.h"

namespace spdlog {
   class logger;
}
struct curl_slist;

namespace bs {
   namespace hww {
      class TrezorDevice;
      class DeviceCallbacks;

      struct TrezorPostIn : public bs::InData
      {
         ~TrezorPostIn() override = default;
         std::string path;
         std::string input;
         bool timeout{ true };
      };
      struct TrezorPostOut : public bs::OutData
      {
         ~TrezorPostOut() override = default;
         std::string    response;
         std::string    error;
      };

      class TrezorPostHandler : public bs::HandlerImpl<TrezorPostIn, TrezorPostOut>
      {
      public:
         TrezorPostHandler(const std::shared_ptr<spdlog::logger>& logger
            , const std::string& baseURL);
         ~TrezorPostHandler() override;

      protected:
         std::shared_ptr<TrezorPostOut> processData(const std::shared_ptr<TrezorPostIn>&) override;

      private:
         std::shared_ptr<spdlog::logger>  logger_;
         const std::string baseURL_;
         struct curl_slist* curlHeaders_{ NULL };
         void* curl_{ nullptr };
      };


      class TrezorClient : protected bs::WorkerPool
      {
         friend class TrezorDevice;
      public:
         TrezorClient(const std::shared_ptr<spdlog::logger>&
            , bool testNet, DeviceCallbacks*);
         ~TrezorClient() override = default;

         void initConnection();
         void releaseConnection();
         void listDevices();

         std::vector<DeviceKey> deviceKeys() const;
         std::shared_ptr<TrezorDevice> getDevice(const std::string& deviceId);

      protected:
         std::shared_ptr<Worker> worker(const std::shared_ptr<InData>&) override final;

      private:
         void acquireDevice(const trezor::DeviceData&, bool init = false);

         //former signals
         void initialized();
         void devicesScanned();
         void deviceReady();
         void deviceReleased();

         void publicKeyReady();
         void onRequestPinMatrix();

      private:
         std::shared_ptr<spdlog::logger>  logger_;
         DeviceCallbacks* cb_{ nullptr };
         const std::string trezorEndPoint_{ "http://127.0.0.1:21325" };
         trezor::State  state_{ trezor::State::None };
         bool testNet_{};
         std::vector<std::shared_ptr<TrezorDevice>>   devices_;
         unsigned int nbDevices_{ 0 };
      };

   }  //hw
}     //bs

#endif // TREZORCLIENT_H
