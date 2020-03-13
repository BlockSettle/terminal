/*

***********************************************************************************
* Copyright (C) 2016 - , BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "hsmdevicemanager.h"
#include "trezorClient.h"
#include "trezorDevice.h"
#include "ConnectionManager.h"

HSMDeviceManager::HSMDeviceManager(const std::shared_ptr<ConnectionManager>& connectionManager, QObject* parent /*= nullptr*/)
   : QObject(parent)
{
   trezorClient_ = std::make_unique<TrezorClient>(connectionManager, this);
   model_ = new QStringListModel(this);
}

void HSMDeviceManager::scanDevices()
{
   trezorClient_->initConnection([this]() {
      devices_.clear();
      devices_.append(trezorClient_->deviceKeys());
      emit devicesChanged();
   });
}

void HSMDeviceManager::requestPublicKey(int deviceIndex)
{
   auto device = trezorClient_->getTrezorDevice(devices_[deviceIndex].deviceId_);
   if (!device) {
      return;
   }

   device->getPublicKey([this]() {
      emit publicKeyReady();
   });

   connect(device, &TrezorDevice::requestPinMatrix,
      this, &HSMDeviceManager::requestPinMatrix, Qt::UniqueConnection);
}

void HSMDeviceManager::setMatrixPin(int deviceIndex, QString pin)
{
   auto device = trezorClient_->getTrezorDevice(devices_[deviceIndex].deviceId_);
   if (!device) {
      return;
   }

   device->setMatrixPin(pin.toStdString());
}

void HSMDeviceManager::cancel(int deviceIndex)
{
   auto device = trezorClient_->getTrezorDevice(devices_[deviceIndex].deviceId_);
   if (!device) {
      return;
   }

   device->cancel();
}

QStringListModel* HSMDeviceManager::devices()
{
   QStringList labels;
   for (const auto& device : devices_)
      labels << device.deviceLabel_;

   model_->setStringList(labels);

   return model_;
}
