/*

***********************************************************************************
* Copyright (C) 2020 - 2021, BlockSettle AB
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
#include "ProtobufHeadlessUtils.h"

using namespace ArmorySigner;

HwDeviceManager::HwDeviceManager(const std::shared_ptr<ConnectionManager>& connectionManager, std::shared_ptr<bs::sync::WalletsManager> walletManager,
   bool testNet, QObject* parent /*= nullptr*/)
   : QObject(parent)
   , logger_(connectionManager->GetLogger())
   , testNet_(testNet)
{
   walletManager_ = walletManager;
   trezorClient_ = std::make_unique<TrezorClient>(connectionManager, walletManager, testNet, this);
   ledgerClient_ = std::make_unique<LedgerClient>(logger_, walletManager, testNet);

   model_ = new HwDeviceModel(this);
}

HwDeviceManager::~HwDeviceManager()
{
   releaseConnection(nullptr);
};

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
      trezorClient_->initConnection(true, [this, doneScanning]() {
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

   connectDevice(device);
}

void HwDeviceManager::setMatrixPin(int deviceIndex, QString pin)
{
   auto device = getDevice(model_->getDevice(deviceIndex));
   if (!device) {
      return;
   }

   device->setMatrixPin(pin.toStdString());
}

void HwDeviceManager::setPassphrase(int deviceIndex, QString passphrase, bool enterOnDevice)
{
   auto device = getDevice(model_->getDevice(deviceIndex));
   if (!device) {
      return;
   }

   device->setPassword(passphrase.toStdString(), enterOnDevice);
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
   bs::wallet::HardwareEncKey hwEncType(encKeys[0]);

   if (bs::wallet::HardwareEncKey::WalletType::Ledger == hwEncType.deviceType()) {
      ledgerClient_->scanDevices([caller = QPointer<HwDeviceManager>(this), walletId]() {
         if (!caller) {
            return;
         }

         auto devices = caller->ledgerClient_->deviceKeys();
         if (devices.empty()) {
            caller->lastOperationError_ = caller->ledgerClient_->lastScanError();
            caller->deviceNotFound(QString::fromStdString(kDeviceLedgerId));
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
            if (!devices.isEmpty()) {
               caller->lastOperationError_ = caller->getDevice(devices.front())->lastError();
            }

            caller->deviceNotFound(QString::fromStdString(kDeviceLedgerId));
         }  
         else {
            caller->model_->resetModel({ std::move(deviceKey) });
            caller->deviceReady(QString::fromStdString(kDeviceLedgerId));
         }
      });
   }
   else if (bs::wallet::HardwareEncKey::WalletType::Trezor == hwEncType.deviceType()) {
      auto deviceId = hwEncType.deviceId();
      const bool cleanPrevSession = (lastUsedTrezorWallet_ != walletId);
      trezorClient_->initConnection(QString::fromStdString(deviceId), cleanPrevSession, [this](QVariant&& deviceId) {
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
      lastUsedTrezorWallet_ = walletId;
   }
}

void HwDeviceManager::signTX(QVariant reqTX)
{
   auto device = getDevice(model_->getDevice(0));
   if (!device) {
      return;
   }

   Blocksettle::Communication::headless::SignTxRequest pbSignReq;
   bool rc = pbSignReq.ParseFromString(reqTX.toByteArray().toStdString());
   if (!rc) {
      SPDLOG_LOGGER_ERROR(logger_, "parse TX failed");
      emit operationFailed(tr("Invalid sign request"));
      return;
   }

   auto signReq = bs::signer::pbTxRequestToCore(pbSignReq);
   auto cbSigned = [this, signReq, device](QVariant&& data) {
      assert(data.canConvert<HWSignedTx>());
      auto tx = data.value<HWSignedTx>();

      if (device->key().type_ == DeviceType::HWTrezor) {
         // According to architecture, Trezor allow us to sign tx with incorrect 
         // passphrase, so let's check that the final tx is correct. In Ledger case
         // this situation is impossible, since the wallets with different passphrase will be treated
         // as different devices, which will be verified in sign part.
         try {
            std::map<BinaryData, std::map<unsigned, UTXO>> utxoMap;
            for (unsigned i=0; i<signReq.armorySigner_.getTxInCount(); i++) {
               const auto& utxo = signReq.armorySigner_.getSpender(i)->getUtxo();
               auto& idMap = utxoMap[utxo.getTxHash()];
               idMap.emplace(utxo.getTxOutIndex(), utxo);
            }
            unsigned flags = SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_SEGWIT | SCRIPT_VERIFY_P2SH_SHA256;
            bool validSign = Signer::verify(SecureBinaryData::fromString(tx.signedTx)
               , utxoMap, flags, true).isValid();
            if (!validSign) {
               SPDLOG_LOGGER_ERROR(logger_, "sign verification failed");
               releaseConnection();
               emit operationFailed(tr("Signing failed. Please ensure you type the correct passphrase."));
               return;
            }
         }
         catch (const std::exception &e) {
            SPDLOG_LOGGER_ERROR(logger_, "sign verification failed: {}", e.what());
            releaseConnection();
            emit operationFailed(tr("Signing failed. Please ensure you type the correct passphrase."));
            return;
         }
      }
      txSigned({ BinaryData::fromString(tx.signedTx) });
   };

   device->signTX(signReq, std::move(cbSigned));

   connectDevice(qobject_cast<HwDeviceInterface*>(device));

   // tx specific connections
   connect(device, &HwDeviceInterface::deviceTxStatusChanged,
      this, &HwDeviceManager::deviceTxStatusChanged, Qt::UniqueConnection);
   connect(device, &HwDeviceInterface::cancelledOnDevice,
      this, &HwDeviceManager::cancelledOnDevice, Qt::UniqueConnection);
   connect(device, &HwDeviceInterface::operationFailed,
      this, &HwDeviceManager::deviceTxStatusChanged, Qt::UniqueConnection);
   connect(device, &HwDeviceInterface::invalidPin,
      this, &HwDeviceManager::invalidPin, Qt::UniqueConnection);
   connect(device, &HwDeviceInterface::requestForRescan,
      this, [this]() {
      auto deviceInfo = model_->getDevice(0);
      lastOperationError_ = getDevice(deviceInfo)->lastError();
      emit deviceNotFound(deviceInfo.deviceId_);
   }, Qt::UniqueConnection);
}

void HwDeviceManager::releaseDevices()
{
   releaseConnection();
}

void HwDeviceManager::hwOperationDone()
{
   model_->resetModel({});
}

bool HwDeviceManager::awaitingUserAction(int deviceIndex)
{
   if (model_->rowCount() <= deviceIndex) {
      return false;
   }

   auto device = getDevice(model_->getDevice(deviceIndex));
   return device && device->isBlocked();
}

QString HwDeviceManager::lastDeviceError()
{
   return lastOperationError_;
}

void HwDeviceManager::releaseConnection(AsyncCallBack&& cb/*= nullptr*/)
{
   for (int i = 0; i < model_->rowCount(); ++i) {
      auto device = getDevice(model_->getDevice(i));
      if (device) {
         trezorClient_->initConnection(true, [this, cbCopy = std::move(cb)] {
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

void HwDeviceManager::scanningDone(bool initDevices /* = true */)
{
   setScanningFlag(false);
   auto allDevices = ledgerClient_->deviceKeys();
   allDevices.append(trezorClient_->deviceKeys());
   model_->resetModel(std::move(allDevices));
   emit devicesChanged();

   if (!initDevices) {
      return;
   }

   for (const auto& key : trezorClient_->deviceKeys()) {
      auto device = trezorClient_->getTrezorDevice(key.deviceId_);
      if (!device->inited()) {
         connectDevice(qobject_cast<HwDeviceInterface*>(device));
         device->retrieveXPubRoot([caller = QPointer<HwDeviceManager>(this)]() {
            if (!caller) {
               return;
            }

            caller->scanningDone(false);
         });
      }
   }
}

void HwDeviceManager::connectDevice(QPointer<HwDeviceInterface> device)
{
   connect(device, &HwDeviceInterface::requestPinMatrix,
      this, &HwDeviceManager::onRequestPinMatrix, Qt::UniqueConnection);
   connect(device, &HwDeviceInterface::requestHWPass,
      this, &HwDeviceManager::onRequestHWPass, Qt::UniqueConnection);
   connect(device, &HwDeviceInterface::operationFailed,
      this, &HwDeviceManager::operationFailed, Qt::UniqueConnection);
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

void HwDeviceManager::onRequestPinMatrix()
{
   auto sender = qobject_cast<HwDeviceInterface *>(QObject::sender());
   int index = model_->getDeviceIndex(sender->key());

   if (index >= 0) {
      emit requestPinMatrix(index);
   }
}

void HwDeviceManager::onRequestHWPass(bool allowedOnDevice)
{
   auto sender = qobject_cast<HwDeviceInterface *>(QObject::sender());
   int index = model_->getDeviceIndex(sender->key());

   if (index >= 0) {
      emit requestHWPass(index, allowedOnDevice);
   }
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
