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
#include "ledger/ledgerClient.h"
#include "ledger/ledgerDevice.h"
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
   ledgerClient_ = std::make_unique<LedgerClient>(connectionManager->GetLogger(), walletManager, testNet);

   model_ = new HwDeviceModel(this);
}

HwDeviceManager::~HwDeviceManager() = default;

void HwDeviceManager::scanDevices()
{
   if (isScanning_) {
      return;
   }


   setScanningFlag(true);
   
   auto doneScanning = [this, expectedClients = 2, finished = std::make_shared<int>(0)]() {
      ++(*finished);

      if (*finished == expectedClients) {
         scanningDone();
      }
   };

   ledgerClient_->scanDevices(doneScanning);
   releaseConnection([this, doneScanning] {
      trezorClient_->initConnection([this, doneScanning]() {
         doneScanning();
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

   connect(device, &HwDeviceInterface::requestPinMatrix,
      this, &HwDeviceManager::requestPinMatrix, Qt::UniqueConnection);
   connect(device, &HwDeviceInterface::requestHWPass,
      this, &HwDeviceManager::requestHWPass, Qt::UniqueConnection);
   connect(device, &HwDeviceInterface::operationFailed,
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

void HwDeviceManager::setPassphrase(int deviceIndex, QString passphrase)
{
   auto device = getDevice(model_->getDevice(deviceIndex));
   if (!device) {
      return;
   }

   device->setPassword(passphrase.toStdString());
}

void HwDeviceManager::cancel(int deviceIndex)
{
   auto device = getDevice(model_->getDevice(deviceIndex));
   if (!device) {
      return;
   }

   device->cancel();
}

void HwDeviceManager::prepareHwDeviceForSign(QString walletId)
{
   auto hdWallet = walletManager_->getHDWalletById(walletId.toStdString());
   assert(hdWallet->isHardwareWallet());
   auto encKeys = hdWallet->encryptionKeys();
   auto deviceId = encKeys[0].toBinStr();

   // #TREZOR_INTEGRATION:  bad way to distinguish device type
   // we need better here
   if (deviceId == kDeviceLedgerId) {
      ledgerClient_->scanDevices([caller = QPointer<HwDeviceManager>(this), deviceId, walletId]() {
         if (!caller) {
            return;
         }

         auto devices = caller->ledgerClient_->deviceKeys();
         if (devices.empty()) {
            caller->deviceNotFound(QString::fromStdString(deviceId));
            return;
         }

         bool found = false;
         DeviceKey deviceKey;
         for (auto Key : devices) {
            if (Key.walletId_ == walletId) {
               deviceKey = Key;
               found = true;
               break;
            }
         }

         if (!found) {
            caller->deviceNotFound(QString::fromStdString(deviceId));
         }  
         else {
            caller->model_->resetModel({ std::move(deviceKey) });
            caller->deviceReady(QString::fromStdString(deviceId));
         }
      });
   }
   else {
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

   connect(device, &HwDeviceInterface::requestPinMatrix,
      this, &HwDeviceManager::requestPinMatrix, Qt::UniqueConnection);
   connect(device, &HwDeviceInterface::requestHWPass,
      this, &HwDeviceManager::requestHWPass, Qt::UniqueConnection);
   connect(device, &HwDeviceInterface::deviceTxStatusChanged,
      this, &HwDeviceManager::deviceTxStatusChanged, Qt::UniqueConnection);
   connect(device, &HwDeviceInterface::cancelledOnDevice,
      this, &HwDeviceManager::cancelledOnDevice, Qt::UniqueConnection);
   connect(device, &HwDeviceInterface::operationFailed,
      this, &HwDeviceManager::deviceTxStatusChanged, Qt::UniqueConnection);
   connect(device, &HwDeviceInterface::requestForRescan,
      this, [this]() {
      auto deviceInfo = model_->getDevice(0);
      emit deviceNotFound(deviceInfo.deviceId_);
   }, Qt::UniqueConnection);
}

void HwDeviceManager::releaseDevices()
{
   releaseConnection();
}

bool HwDeviceManager::awaitingUserAction(int deviceIndex)
{
   auto device = getDevice(model_->getDevice(deviceIndex));
   return device && device->isBlocked();
}

void HwDeviceManager::releaseConnection(AsyncCallBack&& cb/*= nullptr*/)
{
   for (int i = 0; i < model_->rowCount(); ++i) {
      auto device = getDevice(model_->getDevice(i));
      if (device) {
         trezorClient_->initConnection([this, cbCopy = std::move(cb)] {
            trezorClient_->releaseConnection([this, cb = std::move(cbCopy)]() {
               if (cb) {
                  cb();
               }
            });
         });
         model_->resetModel({});
         return;
      }
   }

   if (cb) {
      cb();
   }
}

void HwDeviceManager::scanningDone()
{
   setScanningFlag(false);
   auto allDevices = ledgerClient_->deviceKeys();
   allDevices.append(trezorClient_->deviceKeys());
   model_->resetModel(std::move(allDevices));
   emit devicesChanged();
}

QPointer<HwDeviceInterface> HwDeviceManager::getDevice(DeviceKey key)
{
   switch (key.type_)
   {
   case DeviceType::HWTrezor:
      return static_cast<QPointer<HwDeviceInterface>>(trezorClient_->getTrezorDevice(key.deviceId_));
      break;
   case DeviceType::HWLedger:
      return static_cast<QPointer<HwDeviceInterface>>(ledgerClient_->getDevice(key.deviceId_));
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
