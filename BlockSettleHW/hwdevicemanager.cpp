/*

***********************************************************************************
* Copyright (C) 2020 - 2021, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "hwdevicemanager.h"
#include <spdlog/spdlog.h>
#include "trezor/trezorDevice.h"
#include "ledger/ledgerDevice.h"
#include "TerminalMessage.h"
#include "Wallets/SyncWalletsManager.h"
#include "Wallets/SyncHDWallet.h"
#include "Wallets/ProtobufHeadlessUtils.h"

#include "common.pb.h"
#include "hardware_wallet.pb.h"
#include "terminal.pb.h"

//using namespace Armory::Signer;
using namespace bs::hww;
using namespace BlockSettle::Common;
using namespace BlockSettle::Terminal;
using namespace BlockSettle;

DeviceManager::DeviceManager(const std::shared_ptr<spdlog::logger>& logger)
   : logger_(logger)
   , user_(std::make_shared<bs::message::UserTerminal>(bs::message::TerminalUsers::HWWallets))
   , userWallets_(std::make_shared<bs::message::UserTerminal>(bs::message::TerminalUsers::Wallets))
{}

DeviceManager::~DeviceManager()
{
   releaseConnection();
}

bs::message::ProcessingResult DeviceManager::process(const bs::message::Envelope& env)
{
   if (env.isRequest()) {
      return processOwnRequest(env);
   }
   else {
      switch (env.sender->value<bs::message::TerminalUsers>()) {
      case bs::message::TerminalUsers::Settings:
         return processSettings(env);
      case bs::message::TerminalUsers::Wallets:
         return processWallet(env);
      default: break;
      }
   }
   return bs::message::ProcessingResult::Ignored;
}

bool DeviceManager::processBroadcast(const bs::message::Envelope& env)
{
   if (env.sender->isSystem()) {
      AdministrativeMessage msg;
      if (msg.ParseFromString(env.message)) {
         if (msg.data_case() == AdministrativeMessage::kStart) {
            start();
            return true;
         }
      }
   }
   return false;
}

void DeviceManager::scanDevices(const bs::message::Envelope& env)
{
   if (nbScanning_ > 0) {
      return;
   }
   envReqScan_ = env;
   devices_.clear();
   nbScanning_ = 2;  // # of callbacks to receive
   ledgerClient_->scanDevices();
   trezorClient_->listDevices();
}

void DeviceManager::setMatrixPin(const DeviceKey& key, const std::string& pin)
{
   auto device = getDevice(key);
   if (!device) {
      return;
   }
   device->setMatrixPin(SecureBinaryData::fromString(pin));
}

void DeviceManager::setPassphrase(const DeviceKey& key, const std::string& passphrase
   , bool enterOnDevice)
{
   auto device = getDevice(key);
   if (!device) {
      return;
   }
   device->setPassword(SecureBinaryData::fromString(passphrase), enterOnDevice);
}

void DeviceManager::cancel(const DeviceKey& key)
{
   auto device = getDevice(key);
   if (!device) {
      return;
   }
   device->cancel();
}

void DeviceManager::start()
{
   logger_->debug("[hww::DeviceManager::start]");
   SettingsMessage msg;
   auto msgReq = msg.mutable_get_request();
   auto setReq = msgReq->add_requests();
   setReq->set_source(SettingSource_Local);
   setReq->set_index(SetIdx_NetType);
   setReq->set_type(SettingType_Int);

   pushRequest(user_, std::make_shared<bs::message::UserTerminal>(bs::message::TerminalUsers::Settings)
      , msg.SerializeAsString());
}

bs::message::ProcessingResult DeviceManager::processPrepareDeviceForSign(const bs::message::Envelope& env
   , const std::string& walletId)
{
   WalletsMessage msg;
   msg.set_hd_wallet_get(walletId);
   const auto msgId = pushRequest(user_, userWallets_, msg.SerializeAsString());
   prepareDeviceReq_[msgId] = {walletId, env};
   return bs::message::ProcessingResult::Success;
}

bs::message::ProcessingResult DeviceManager::processOwnRequest(const bs::message::Envelope& env)
{
   if (!trezorClient_ || !ledgerClient_) {
      return bs::message::ProcessingResult::Retry;
   }
   HW::DeviceMgrMessage msg;
   if (!msg.ParseFromString(env.message)) {
      logger_->error("[hww::DeviceManager::processOwnRequest] failed to parse #{}"
         , env.foreignId());
      return bs::message::ProcessingResult::Error;
   }
   switch (msg.data_case()) {
   case HW::DeviceMgrMessage::kStartScan:
      scanDevices(env);
      return bs::message::ProcessingResult::Success;
   case HW::DeviceMgrMessage::kImportDevice:
      return processImport(env, msg.import_device());
   default: break;
   }
   return bs::message::ProcessingResult::Ignored;
}

bs::message::ProcessingResult DeviceManager::processWallet(const bs::message::Envelope& env)
{
   WalletsMessage msg;
   if (!msg.ParseFromString(env.message)) {
      logger_->error("[hww::DeviceManager::processWallet] failed to parse #{}"
         , env.foreignId());
      return bs::message::ProcessingResult::Error;
   }
   switch (msg.data_case()) {
   case WalletsMessage::kHdWallet:
      return prepareDeviceForSign(env.responseId(), msg.hd_wallet());
   }
   return bs::message::ProcessingResult::Ignored;
}

bs::message::ProcessingResult DeviceManager::processSettings(const bs::message::Envelope& env)
{
   SettingsMessage msg;
   if (!msg.ParseFromString(env.message)) {
      logger_->error("[hww::DeviceManager::processSettings] failed to parse #{}"
         , env.foreignId());
      return bs::message::ProcessingResult::Error;
   }
   if (msg.data_case() == SettingsMessage::kGetResponse) {
      for (const auto& setting : msg.get_response().responses()) {
         if (setting.request().index() == SetIdx_NetType) {
            testNet_ = (static_cast<NetworkType>(setting.i()) == NetworkType::TestNet);
            logger_->debug("[hww::DeviceManager::processSettings] testnet={}", testNet_);
            trezorClient_ = std::make_unique<TrezorClient>(logger_, testNet_, this);
            ledgerClient_ = std::make_unique<LedgerClient>(logger_, testNet_, this);
            return bs::message::ProcessingResult::Success;
         }
      }
   }
   return bs::message::ProcessingResult::Ignored;
}

bs::message::ProcessingResult DeviceManager::processImport(const bs::message::Envelope& env
   , const HW::DeviceKey& key)
{
   const DeviceKey devKey{ key.label(), key.id(), key.vendor(), key.wallet_id()
         , key.status(), static_cast<bs::hww::DeviceType>(key.type()) };
   const auto& device = getDevice(devKey);
   if (!device) {
      logger_->error("[hww::DeviceManager::processImport] no device found for id {}"
         , devKey.id);
      return bs::message::ProcessingResult::Error;
   }
   device->getPublicKeys();
   return bs::message::ProcessingResult::Success;
}

bs::message::ProcessingResult DeviceManager::prepareDeviceForSign(bs::message::SeqId msgId
   , const HDWalletData& hdWallet)
{
   const auto& itWallet = prepareDeviceReq_.find(msgId);
   if (itWallet == prepareDeviceReq_.end()) {
      logger_->warn("[{}] unknown response #{}", __func__, msgId);
      return bs::message::ProcessingResult::Error;
   }
   prepareDeviceReq_.erase(itWallet);
   if (!hdWallet.is_hardware() || !hdWallet.encryption_keys_size()) {
      logger_->error("[{}] wallet {} is not suitable", __func__, hdWallet.wallet_id());
      return bs::message::ProcessingResult::Error;

   }
   bs::wallet::HardwareEncKey hwEncType(BinaryData::fromString(hdWallet.encryption_keys(0)));

   if (bs::wallet::HardwareEncKey::WalletType::Ledger == hwEncType.deviceType()) {
      ledgerClient_->scanDevices();
      const auto& devices = ledgerClient_->deviceKeys();
      if (devices.empty()) {
         lastOperationError_ = ledgerClient_->lastScanError();
         deviceNotFound(kDeviceLedgerId);
         return bs::message::ProcessingResult::Error;
      }

      bool found = false;
      DeviceKey deviceKey;
      for (const auto& key : devices) {
         if (key.walletId == hdWallet.wallet_id()) {
            deviceKey = key;
            found = true;
            break;
         }
      }
      if (!found) {
         if (!devices.empty()) {
            lastOperationError_ = getDevice(devices.front())->lastError();
         }
         deviceNotFound(kDeviceLedgerId);
      }  
      else {
         deviceReady(kDeviceLedgerId);
      }
   }
   else if (bs::wallet::HardwareEncKey::WalletType::Trezor == hwEncType.deviceType()) {
      auto deviceId = hwEncType.deviceId();
      const bool cleanPrevSession = (lastUsedTrezorWallet_ != hdWallet.wallet_id());
      {
         DeviceKey deviceKey;
         bool found = false;
         for (auto key : trezorClient_->deviceKeys()) {
            if (key.id == deviceId) {
               found = true;
               deviceKey = key;
               break;
            }
         }

         if (!found) {
            deviceNotFound(deviceId);
         }
         else {
            deviceReady(deviceId);
         }
      }
      lastUsedTrezorWallet_ = hdWallet.wallet_id();
   }
}

void DeviceManager::signTX(const DeviceKey& key, const bs::core::wallet::TXSignRequest& signReq)
{
   auto device = getDevice(key);
   if (!device) {
      deviceNotFound(key.id);
      return;
   }
   device->signTX(signReq);
}

void DeviceManager::releaseDevices()
{
   releaseConnection();
}

bool DeviceManager::awaitingUserAction(const DeviceKey& key)
{
   const auto& device = getDevice(key);
   return device && device->isBlocked();
}

void DeviceManager::releaseConnection()
{
   if (trezorClient_) {
      trezorClient_->releaseConnection();
   }
}

void DeviceManager::devicesResponse()
{
   HW::DeviceMgrMessage msg;
   auto msgResp = msg.mutable_available_devices();
   for (const auto& key : devices_) {
      auto msgKey = msgResp->add_device_keys();
      msgKey->set_label(key.label);
      msgKey->set_id(key.id);
      msgKey->set_vendor(key.vendor);
      msgKey->set_wallet_id(key.walletId);
      msgKey->set_status(key.status);
      msgKey->set_type((int)key.type);
   }
   pushResponse(user_, envReqScan_, msg.SerializeAsString());
   envReqScan_ = {};
}

void DeviceManager::scanningDone(bool initDevices)
{
   const auto& ledgerKeys = ledgerClient_->deviceKeys();
   devices_ = ledgerKeys;
   const auto& trezorKeys = trezorClient_->deviceKeys();
   devices_.insert(devices_.end(), trezorKeys.cbegin(), trezorKeys.cend());

   if (!initDevices) {
      devicesResponse();
      return;
   }
   for (const auto& key : ledgerKeys) {
      auto device = ledgerClient_->getDevice(key.id);
      if (!device->inited()) {
         device->init();
      }
   }
   for (const auto& key : trezorKeys) {
      auto device = trezorClient_->getDevice(key.id);
      if (!device->inited()) {
         device->retrieveXPubRoot();
      }
   }
}

std::shared_ptr<DeviceInterface> DeviceManager::getDevice(const DeviceKey& key) const
{
   switch (key.type)
   {
   case DeviceType::HWTrezor:
      return trezorClient_->getDevice(key.id);
   case DeviceType::HWLedger:
      return ledgerClient_->getDevice(key.id);
   default:
      // Add new device type
      assert(false);
      break;
   }
   return nullptr;
}

void DeviceManager::publicKeyReady(const std::string& devId, const std::string& walletId)
{
   size_t nbCompleted = 0;
   for (int i = 0; i < devices_.size(); ++i) {
      if (devices_[i].id == devId) {
         devices_[i].walletId = walletId;
         nbCompleted++;
      }
      else if (!devices_.at(i).walletId.empty()) {
         nbCompleted++;
      }
   }
   if (nbCompleted >= devices_.size()) {
      devicesResponse();
   }
}

void bs::hww::DeviceManager::walletInfoReady(const DeviceKey& key
   , const bs::core::wallet::HwWalletInfo& walletInfo)
{
   logger_->debug("[hww::DeviceManager::walletInfoReady] importing device {} "
      "(not implemented)", key.id);
   //TODO: send request to WA
}

void DeviceManager::requestPinMatrix(const DeviceKey&)
{
}

void DeviceManager::requestHWPass(const DeviceKey&, bool allowedOnDevice)
{
}

void DeviceManager::deviceNotFound(const std::string& deviceId)
{
}

void DeviceManager::deviceReady(const std::string& deviceId)
{
}

void DeviceManager::deviceTxStatusChanged(const std::string& status)
{
}

void DeviceManager::txSigned(const SecureBinaryData& signData)
{
}

void DeviceManager::scanningDone()
{
   if (nbScanning_ == 0) {
      logger_->error("[DeviceManager::scanningDone] more scanning done events than expected");
      return;
   }
   if (--nbScanning_ == 0) {
      logger_->debug("[DeviceManager::scanningDone] all devices scanned");
      scanningDone(true);
   }
}

void DeviceManager::operationFailed(const std::string& deviceId, const std::string& reason)
{
}

void DeviceManager::cancelledOnDevice()
{
}

void DeviceManager::invalidPin()
{
}

using namespace bs::hd;

namespace bs {
   namespace hww {
      Path getDerivationPath(bool testNet, Purpose element)
      {
         Path path;
         path.append(hardFlag | element);
         path.append(testNet ? CoinType::Bitcoin_test : CoinType::Bitcoin_main);
         path.append(hardFlag);
         return path;
      }

      bool isNestedSegwit(const bs::hd::Path& path)
      {
         return path.get(0) == (bs::hd::Purpose::Nested | bs::hd::hardFlag);
      }

      bool isNativeSegwit(const bs::hd::Path& path)
      {
         return path.get(0) == (bs::hd::Purpose::Native | bs::hd::hardFlag);
      }

      bool isNonSegwit(const bs::hd::Path& path)
      {
         return path.get(0) == (bs::hd::Purpose::NonSegWit | bs::hd::hardFlag);
      }
   }  //hww
}     //bs
