/*

***********************************************************************************
* Copyright (C) 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "jadeClient.h"
#include "hwdevicemanager.h"
#include "jadeDevice.h"
#include "SystemFileUtils.h"
#include "Wallets/SyncWalletsManager.h"
#include "Wallets/SyncHDWallet.h"


using namespace bs::hww;
using json = nlohmann::json;

JadeClient::JadeClient(const std::shared_ptr<spdlog::logger>& logger
   , bool testNet, DeviceCallbacks* cb)
   : logger_(logger), cb_(cb), testNet_(testNet)
{}

void JadeClient::initConnection()
{
   logger_->info("[JadeClient::initConnection]");
}

std::vector<DeviceKey> JadeClient::deviceKeys() const
{
   std::vector<DeviceKey> result;
   for (const auto& device : devices_) {
      result.push_back(device->key());
   }
   return result;
}

std::shared_ptr<JadeDevice> JadeClient::getDevice(const std::string& deviceId)
{
   for (const auto& device : devices_) {
      if (device->key().id == deviceId) {
         return device;
      }
   }
   return nullptr;
}

void JadeClient::scanDevices()
{
   logger_->info("[JadeClient::scanDevices]");
   QMetaObject::invokeMethod(qApp, [this] {
      for (const auto& serial : QSerialPortInfo::availablePorts()) {
         const auto& it = std::find_if(devices_.cbegin(), devices_.cend()
            , [serial](const std::shared_ptr<JadeDevice>& dev) {
               return JadeDevice::idFromSerial(serial) == dev->key().id; });
         if (it != devices_.end()) {
            continue;
         }
         logger_->debug("[JadeClient::scanDevices] probing {}/{}", serial.portName().toStdString()
            , serial.manufacturer().toStdString());
         try {
            const auto& device = std::make_shared<JadeDevice>(logger_, testNet_
               , cb_, serial);
            devices_.push_back(device);
         }
         catch (const std::exception& e) {
            logger_->error("[JadeClient::scanDevices] {}", e.what());
         }
      }
      logger_->debug("[JadeClient::scanDevices] {} devices scanned", devices_.size());
      cb_->scanningDone();
   });
}

bool bs::hww::JadeClient::isConnected(const std::string& reqId) const
{
   for (const auto& serial : QSerialPortInfo::availablePorts()) {
      const auto& id = JadeDevice::idFromSerial(serial);
      if (id == reqId) {
         return true;
      }
   }
   return false;
}
