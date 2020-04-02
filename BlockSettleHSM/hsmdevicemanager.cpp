/*

***********************************************************************************
* Copyright (C) 2016 - , BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "hsmdevicemanager.h"
#include "trezor/trezorClient.h"
#include "trezor/trezorDevice.h"
#include "ledger/ledgerClient.h"
#include "ledger/ledgerDevice.h"
#include "ConnectionManager.h"
#include "WalletManager.h"

HSMDeviceManager::HSMDeviceManager(const std::shared_ptr<ConnectionManager>& connectionManager, std::shared_ptr<bs::sync::WalletsManager> walletManager,
   bool testNet, QObject* parent /*= nullptr*/)
   : QObject(parent)
   , testNet_(testNet)
{
   trezorClient_ = std::make_unique<TrezorClient>(connectionManager, walletManager, testNet, this);
   ledgerClient_ = std::make_unique<LedgerClient>(connectionManager->GetLogger(), testNet);

   model_ = new HSMDeviceModel(this);
}

HSMDeviceManager::~HSMDeviceManager() = default;

void HSMDeviceManager::scanDevices()
{
   if (isScanning_) {
      return;
   }

   setScanningFlag(true);

   ledgerClient_->scanDevicesNonBlocking();
   releaseConnection([this] {
      trezorClient_->initConnection([this]() {
         setScanningFlag(false);
         auto allDevices = ledgerClient_->deviceKeys();
         allDevices.append(trezorClient_->deviceKeys());
         model_->resetModel(std::move(allDevices));
         emit devicesChanged();
      });
   });
}

void HSMDeviceManager::requestPublicKey(int deviceIndex)
{
   auto device = getDevice(model_->getDevice(deviceIndex));
   if (!device) {
      return;
   }

   device->getPublicKey([this](QVariant&& data) {
      emit publicKeyReady(data);
   });

   connect(device, &TrezorDevice::requestPinMatrix,
      this, &HSMDeviceManager::requestPinMatrix, Qt::UniqueConnection);
   connect(device, &TrezorDevice::operationFailed,
      this, &HSMDeviceManager::operationFailed, Qt::UniqueConnection);
}

void HSMDeviceManager::setMatrixPin(int deviceIndex, QString pin)
{
   auto device = getDevice(model_->getDevice(deviceIndex));
   if (!device) {
      return;
   }

   device->setMatrixPin(pin.toStdString());
}

void HSMDeviceManager::cancel(int deviceIndex)
{
   auto device = getDevice(model_->getDevice(deviceIndex));
   if (!device) {
      return;
   }

   device->cancel();
}

void HSMDeviceManager::prepareTrezorForSign(QString walleiId)
{
   trezorClient_->initConnection(walleiId, [this](QVariant&& deviceId) {
      DeviceKey deviceKey;

      const auto id = deviceId.toString();

      bool found = false;
      for (auto key : trezorClient_->deviceKeys()) {
         if (key.deviceId_ == id) {
            found = true;
            deviceKey = key;
            break;
         }
      }

      if (!found) {
         emit deviceNotFound(id);
      }
      else {
         model_->resetModel({ std::move(deviceKey) });
         emit deviceReady(id);
      }
   });
}

void HSMDeviceManager::signTX(QVariant reqTX)
{
   auto device = getDevice(model_->getDevice(0));
   if (!device) {
      return;
   }

   device->signTX(reqTX, [this](QVariant&& data) {
      assert(data.canConvert<HSMSignedTx>());
      auto tx = data.value<HSMSignedTx>();
      txSigned({ BinaryData::fromString(tx.signedTx) });
      releaseDevices();
   });

   connect(device, &TrezorDevice::requestPinMatrix,
      this, &HSMDeviceManager::requestPinMatrix, Qt::UniqueConnection);
   connect(device, &TrezorDevice::deviceTxStatusChanged,
      this, &HSMDeviceManager::deviceTxStatusChanged, Qt::UniqueConnection);
}

void HSMDeviceManager::releaseDevices()
{
   releaseConnection();
}

void HSMDeviceManager::releaseConnection(AsyncCallBack&& cb/*= nullptr*/)
{
   for (int i = 0; i < model_->rowCount(); ++i) {
      auto device = getDevice(model_->getDevice(i));
      if (device) {
         device->init([this, cbCopy = std::move(cb)]() mutable {
            trezorClient_->releaseConnection(std::move(cbCopy));
         });
         model_->resetModel({});
         return;
      }
   }

   if (cb) {
      cb();
   }
}

QPointer<HSMDeviceAbstract> HSMDeviceManager::getDevice(DeviceKey key)
{
   switch (key.type_)
   {
   case DeviceType::HWTrezor:
      return trezorClient_->getTrezorDevice(key.deviceId_);
      break;
   case DeviceType::HWLedger:
      return ledgerClient_->getDevice(key.deviceId_);
      break;
   default:
      // Add new device type
      assert(false);
      break;
   }

   return nullptr;
}

void HSMDeviceManager::setScanningFlag(bool isScanning)
{
   if (isScanning_ == isScanning) {
      return;
   }

   isScanning_ = isScanning;
   emit isScanningChanged();
}

HSMDeviceModel* HSMDeviceManager::devices()
{
   return model_;
}

bool HSMDeviceManager::isScanning() const
{
   return isScanning_;
}
