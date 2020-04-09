/*

***********************************************************************************
* Copyright (C) 2016 - , BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "hwdevicemanager.h"
#include "trezor/trezorClient.h"
#include "trezor/trezorDevice.h"
#include "ConnectionManager.h"
#include "WalletManager.h"
#include "Wallets/SyncWalletsManager.h"
#include "Wallets/SyncHDWallet.h"

HwDeviceManager::HwDeviceManager(const std::shared_ptr<ConnectionManager>& connectionManager, std::shared_ptr<bs::sync::WalletsManager> walletManager,
   bool testNet, QObject* parent /*= nullptr*/)
   : QObject(parent)
   , testNet_(testNet)
{
   walletManager_ = walletManager;
   trezorClient_ = std::make_unique<TrezorClient>(connectionManager, walletManager, testNet, this);

   model_ = new HwDeviceModel(this);
}

HwDeviceManager::~HwDeviceManager() = default;

void HwDeviceManager::scanDevices()
{
   if (isScanning_) {
      return;
   }

   setScanningFlag(true);

   releaseConnection([this] {
      trezorClient_->initConnection([this]() {
         setScanningFlag(false);
         model_->resetModel(trezorClient_->deviceKeys());
         emit devicesChanged();
      });
   });
}

void HwDeviceManager::requestPublicKey(int deviceIndex)
{
   auto device = getDevice(model_->getDevice(deviceIndex));
   if (!device) {
      return;
   }

   device->getPublicKey([this](QVariant&& data) {
      emit publicKeyReady(data);
   });

   connect(device, &TrezorDevice::requestPinMatrix,
      this, &HwDeviceManager::requestPinMatrix, Qt::UniqueConnection);
   connect(device, &TrezorDevice::operationFailed,
      this, &HwDeviceManager::operationFailed, Qt::UniqueConnection);
}

void HwDeviceManager::setMatrixPin(int deviceIndex, QString pin)
{
   auto device = getDevice(model_->getDevice(deviceIndex));
   if (!device) {
      return;
   }

   device->setMatrixPin(pin.toStdString());
}

void HwDeviceManager::cancel(int deviceIndex)
{
   auto device = getDevice(model_->getDevice(deviceIndex));
   if (!device) {
      return;
   }

   device->cancel();
}

void HwDeviceManager::prepareHwDeviceForSign(QString walleiId)
{
   auto hdWallet = walletManager_->getHDWalletById(walleiId.toStdString());
   assert(hdWallet->isHardwareWallet());
   auto encKeys = hdWallet->encryptionKeys();
   auto deviceId = encKeys[0].toBinStr();

   trezorClient_->initConnection(QString::fromStdString(deviceId), [this](QVariant&& deviceId) {
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

void HwDeviceManager::signTX(QVariant reqTX)
{
   auto device = getDevice(model_->getDevice(0));
   if (!device) {
      return;
   }

   device->signTX(reqTX, [this](QVariant&& data) {
      assert(data.canConvert<HWSignedTx>());
      auto tx = data.value<HWSignedTx>();
      txSigned({ BinaryData::fromString(tx.signedTx) });
      releaseDevices();
   });

   connect(device, &TrezorDevice::requestPinMatrix,
      this, &HwDeviceManager::requestPinMatrix, Qt::UniqueConnection);
   connect(device, &TrezorDevice::deviceTxStatusChanged,
      this, &HwDeviceManager::deviceTxStatusChanged, Qt::UniqueConnection);
}

void HwDeviceManager::releaseDevices()
{
   releaseConnection();
}

void HwDeviceManager::releaseConnection(AsyncCallBack&& cb/*= nullptr*/)
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

QPointer<HwDeviceInterface> HwDeviceManager::getDevice(DeviceKey key)
{
   switch (key.type_)
   {
   case DeviceType::HWTrezor:
      return static_cast<QPointer<HwDeviceInterface>>(trezorClient_->getTrezorDevice(key.deviceId_));
      break;
   case DeviceType::HWLedger:
      break;
   default:
      // Add new device type
      assert(false);
      break;
   }

   return nullptr;
}

void HwDeviceManager::setScanningFlag(bool isScanning)
{
   if (isScanning_ == isScanning) {
      return;
   }

   isScanning_ = isScanning;
   emit isScanningChanged();
}

HwDeviceModel* HwDeviceManager::devices()
{
   return model_;
}

bool HwDeviceManager::isScanning() const
{
   return isScanning_;
}
