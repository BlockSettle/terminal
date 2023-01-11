/*

***********************************************************************************
* Copyright (C) 2020 - 2021, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifdef WIN32
#pragma comment (lib, "Setupapi.lib")
#include <windows.h>
#include <setupapi.h>
#endif
#include <QByteArray>
#include <QString>
#include <spdlog/logger.h>
#include "ledger/ledgerClient.h"
#include "ledger/ledgerDevice.h"
#include "ledger/hidapi/hidapi.h"
#include "hwdevicemanager.h"

using namespace bs::hww;

namespace {
   HidDeviceInfo fromHidOriginal(hid_device_info* info) {
      return {
         QString::fromUtf8(info->path).toStdString(),
         info->vendor_id, 
         info->product_id,
         QString::fromWCharArray(info->serial_number).toStdString(),
         info->release_number,
         QString::fromWCharArray(info->manufacturer_string).toStdString(),
         QString::fromWCharArray(info->product_string).toStdString(),
         info->usage_page,
         info->usage,
         info->interface_number,
      };
   }
}

LedgerClient::LedgerClient(const std::shared_ptr<spdlog::logger>& logger
   , bool testNet, DeviceCallbacks *cb)
   : logger_(logger), testNet_(testNet), cb_(cb)
{
   hidLock_ = std::make_shared<std::mutex>();
}

std::vector<DeviceKey> LedgerClient::deviceKeys() const
{
   std::vector<DeviceKey> keys;
   keys.reserve(availableDevices_.size());
   for (const auto device : availableDevices_) {
      if (device->inited()) {
         keys.push_back(device->key());
      }
   }
   return keys;
}

std::shared_ptr<DeviceInterface> LedgerClient::getDevice(const std::string& deviceId)
{
   for (auto device : availableDevices_) {
      if (device->key().id == deviceId) {
         return device;
      }
   }
   return nullptr;
}

std::string LedgerClient::lastScanError() const
{
   return lastScanError_;
}

void LedgerClient::scanDevices()
{
   availableDevices_.clear();

   hid_device_info* info = hid_enumerate(0, 0);
   for (; info; info = info->next) {
      if (checkLedgerDevice(info)) {
         auto device = std::make_shared<LedgerDevice>(fromHidOriginal(info), testNet_, logger_, cb_, hidLock_);
         availableDevices_.push_back({ device });
      }
   }

   if (availableDevices_.empty()) {
      logger_->error("[LedgerClient::scanDevices] no ledger device available");
   }
   else {
      logger_->info("[LedgerClient::scanDevices] found {} device[s]", availableDevices_.size());
   }
   hid_exit();

   cb_->scanningDone();
}
