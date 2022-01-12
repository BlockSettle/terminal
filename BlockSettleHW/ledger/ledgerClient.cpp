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

#include "ledger/ledgerClient.h"
#include "ledger/ledgerDevice.h"
#include "ledger/hidapi/hidapi.h"
#include <spdlog/logger.h>
#include "Wallets/SyncWalletsManager.h"

#include <QString>
#include <QByteArray>


namespace {
   HidDeviceInfo fromHidOriginal(hid_device_info* info) {
      return {
         QString::fromUtf8(info->path),
         info->vendor_id, 
         info->product_id,
         QString::fromWCharArray(info->serial_number),
         info->release_number,
         QString::fromWCharArray(info->manufacturer_string),
         QString::fromWCharArray(info->product_string),
         info->usage_page,
         info->usage,
         info->interface_number,
      };
   }
}

LedgerClient::LedgerClient(std::shared_ptr<spdlog::logger> logger, std::shared_ptr<bs::sync::WalletsManager> walletManager, bool testNet, QObject *parent /*= nullptr*/)
   : QObject(parent)
   , logger_(logger)
   , testNet_(testNet)
   , walletManager_(walletManager)
{
   hidLock_ = std::make_shared<std::mutex>();
}

QVector<DeviceKey> LedgerClient::deviceKeys() const
{
   QVector<DeviceKey> keys;
   keys.reserve(availableDevices_.size());
   for (const auto device : availableDevices_) {
      if (device->inited()) {
         keys.push_back(device->key());
      }
   }
   return keys;
}

QPointer<LedgerDevice> LedgerClient::getDevice(const QString& deviceId)
{
   for (auto device : availableDevices_) {
      if (device->key().deviceId_ == deviceId) {
         return device;
      }
   }

   return nullptr;
}

QString LedgerClient::lastScanError() const
{
   return lastScanError_;
}

void LedgerClient::scanDevices(AsyncCallBack&& cb)
{
   availableDevices_.clear();

   hid_device_info* info = hid_enumerate(0, 0);
   for (; info; info = info->next) {
      if (checkLedgerDevice(info)) {
         auto device = new LedgerDevice{ fromHidOriginal(info), testNet_, walletManager_, logger_, this, hidLock_};
         availableDevices_.push_back({ device });
      }
   }

   if (availableDevices_.empty()) {
      logger_->error(
         "[LedgerClient] scanDevices - No ledger device available");
   }
   else {
      logger_->info(
         "[LedgerClient] scanDevices - Enumerate request succeeded. Total device available : "
         + QString::number(availableDevices_.size()).toUtf8() + ".");
   }

   hid_exit();

   // Init first one
   if (availableDevices_.empty()) {
      if (cb) {
         cb();
      }
   }
   else {
      auto cbSaveScanError = [caller = QPointer<LedgerClient>(this), cbCopy = std::move(cb)]() {
         caller->lastScanError_ = caller->availableDevices_[0]->lastError();

         if (cbCopy) {
            cbCopy();
         }
      };

      availableDevices_[0]->init(std::move(cbSaveScanError));
   }
}
