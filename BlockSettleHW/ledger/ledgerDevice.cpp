/*

***********************************************************************************
* Copyright (C) 2020 - 2021, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "spdlog/logger.h"
#include "ledger/ledgerDevice.h"
#include "ledger/ledgerClient.h"
#include "Assets.h"
#include "CoreWallet.h"
#include "hwdevicemanager.h"
#include "Wallets/ProtobufHeadlessUtils.h"
#include "Wallets/SyncWalletsManager.h"
#include "Wallets/SyncHDWallet.h"
#include "ScopeGuard.h"

#include "QByteArray"

namespace {
   const uint16_t kHidapiBrokenSequence = 191;
   const std::string kHidapiSequence191 = "Unexpected sequence number 191";
   int sendApdu(hid_device* dongle, const QByteArray& command) {
      int result = 0;
      QVector<QByteArray> chunks;
      uint16_t chunkNumber = 0;

      QByteArray current;
      current.reserve(Ledger::CHUNK_MAX_BLOCK);
      writeUintBE(current, Ledger::CHANNEL);
      writeUintBE(current, Ledger::TAG_APDU);
      writeUintBE(current, chunkNumber++);

      writeUintBE(current, static_cast<uint16_t>(command.size()));


      current.append(command.mid(0, Ledger::FIRST_BLOCK_SIZE));
      chunks.push_back(std::move(current));

      int processed = std::min(static_cast<int>(Ledger::FIRST_BLOCK_SIZE), chunks.first().size());
      for (; command.size() - processed > 0; processed += Ledger::NEXT_BLOCK_SIZE) {
         current.clear();
         writeUintBE(current, Ledger::CHANNEL);
         writeUintBE(current, Ledger::TAG_APDU);
         writeUintBE(current, chunkNumber++);

         current.append(command.mid(processed, Ledger::NEXT_BLOCK_SIZE));
         chunks.push_back(std::move(current));
      }

      chunks.last() = chunks.last().leftJustified(Ledger::CHUNK_MAX_BLOCK, 0x00);

      for (auto &chunk : chunks) {
         assert(chunk.size() == Ledger::CHUNK_MAX_BLOCK);
         chunk.prepend(static_cast<char>(0x00));
         result = hid_write(dongle, reinterpret_cast<unsigned char*>(chunk.data())
            , Ledger::CHUNK_MAX_BLOCK + 1);

         if (result < 0) {
            break;
         }
      }

      return result;
   }

   uint16_t receiveApduResult(hid_device* dongle, QByteArray& response) {
      response.clear();
      uint16_t expectedChunkIndex = 0;

      unsigned char buf[Ledger::CHUNK_MAX_BLOCK];
      int result = hid_read(dongle, buf, Ledger::CHUNK_MAX_BLOCK);
      if (result < 0) {
         return result;
      }

      QByteArray chunk(reinterpret_cast<char*>(buf), Ledger::CHUNK_MAX_BLOCK);
      auto checkChunkIndex = [&chunk, &expectedChunkIndex]() {
         auto chunkIndex = static_cast<uint16_t>(((uint8_t)chunk[3] << 8) | (uint8_t)chunk[4]);
         if (chunkIndex != expectedChunkIndex++) {
            if (chunkIndex == static_cast<uint16_t>(kHidapiBrokenSequence)) {
               throw std::logic_error(kHidapiSequence191);
            }
            else {
               throw std::logic_error("Unexpected sequence number");
            }
         }
      };

      checkChunkIndex();
      int left = static_cast<int>(((uint8_t)chunk[5] << 8) | (uint8_t)chunk[6]);

      response.append(chunk.mid(Ledger::FIRST_BLOCK_OFFSET, left));
      left -= Ledger::FIRST_BLOCK_SIZE;

      for (; left > 0; left -= Ledger::NEXT_BLOCK_SIZE) {
         chunk.clear();
         int result = hid_read(dongle, buf, Ledger::CHUNK_MAX_BLOCK);
         if (result < 0) {
            return result;
         }

         chunk = QByteArray(reinterpret_cast<char*>(buf), Ledger::CHUNK_MAX_BLOCK);
         checkChunkIndex();

         response.append(chunk.mid(Ledger::NEXT_BLOCK_OFFSET, left));
      }

      auto resultCode = response.right(2);
      response.chop(2);
      return static_cast<uint16_t>(((uint8_t)resultCode[0] << 8) | (uint8_t)resultCode[1]);

   }

   QByteArray getApduHeader(uint8_t cla, uint8_t ins, uint8_t p1, uint8_t p2) {
      QByteArray header;
      header.append(cla);
      header.append(ins);
      header.append(p1);
      header.append(p2);
      return header;
   }

   QByteArray getApduCommand(uint8_t cla, uint8_t ins, uint8_t p1, uint8_t p2, QByteArray&& payload) {
      QByteArray command = getApduHeader(cla, ins, p1, p2);
      command.append(static_cast<char>(payload.size()));
      command.append(payload);
      return command;
   }
}

using namespace bs::hww;

LedgerDevice::LedgerDevice(const HidDeviceInfo& hidDeviceInfo, bool testNet
   , const std::shared_ptr<spdlog::logger> &logger, DeviceCallbacks* cb
   , const std::shared_ptr<std::mutex>& hidLock)
   : WorkerPool(1, 1), hidDeviceInfo_{hidDeviceInfo}
   , logger_{logger}, cb_(cb), testNet_{testNet}, hidLock_{hidLock}
{}

DeviceKey LedgerDevice::key() const
{
   std::string walletId;
   if (!xpubRoot_.empty()) {
      walletId = bs::core::wallet::computeID(xpubRoot_).toBinStr();
   }

   return { hidDeviceInfo_.product, hidDeviceInfo_.serialNumber
      , hidDeviceInfo_.manufacturer, walletId, {}, DeviceType::HWLedger };
}

DeviceType LedgerDevice::type() const
{
   return DeviceType::HWLedger;
}

void LedgerDevice::init()
{
   retrieveXPubRoot();
}


bool DeviceIOHandler::writeData(const QByteArray& input, const std::string& logHeader)
{
   logger_->debug("{} - >>> {}", logHeader, input.toHex().toStdString());
   if (sendApdu(dongle_, input) < 0) {
      logger_->error("{} - Cannot write to device.", logHeader);
      throw std::runtime_error("Cannot write to device");
   }
   return true;
}

bool DeviceIOHandler::readData(QByteArray& output, const std::string& logHeader)
{
   const auto res = receiveApduResult(dongle_, output);
   if (res != Ledger::SW_OK) {
      logger_->error("{} - Cannot read from device. APDU error code : {}",
         logHeader, QByteArray::number(res, 16).toStdString());
      throw std::runtime_error("Can't read from device: " + std::to_string(res));
   }
   logger_->debug("{} - <<< {}", logHeader, BinaryData::fromString(output.toStdString()).toHexStr() + "9000");
   return true;
}

bool DeviceIOHandler::initDevice()
{
   if (hid_init() < 0 || hidDeviceInfo_.serialNumber.empty()) {
      logger_->info("[DeviceIOHandler::initDevice] cannot init hid");
      return false;
   }

   {  // make sure that user does not switch off device in the middle of operation
      auto* info = hid_enumerate(hidDeviceInfo_.vendorId, hidDeviceInfo_.productId);
      if (!info) {
         return false;
      }

      bool bFound = false;
      for (; info; info = info->next) {
         if (checkLedgerDevice(info)) {
            bFound = true;
            break;
         }
      }

      if (!bFound) {
         return false;
      }
   }

   std::unique_ptr<wchar_t> serNumb(new wchar_t[hidDeviceInfo_.serialNumber.length() + 1]);
   QString::fromStdString(hidDeviceInfo_.serialNumber).toWCharArray(serNumb.get());
   serNumb.get()[hidDeviceInfo_.serialNumber.length()] = 0x00;
   dongle_ = nullptr;
   dongle_ = hid_open(hidDeviceInfo_.vendorId, static_cast<ushort>(hidDeviceInfo_.productId), serNumb.get());

   return dongle_ != nullptr;
}

void DeviceIOHandler::releaseDevice()
{
   if (dongle_) {
      hid_close(dongle_);
      hid_exit();
      dongle_ = nullptr;
   }
}

bool DeviceIOHandler::exchangeData(const QByteArray& input, QByteArray& output
   , std::string&& logHeader)
{
   if (!writeData(input, logHeader)) {
      return false;
   }

   try {
      return readData(output, logHeader);
   }
   catch (std::exception& e) {
      // Special case : sometimes hidapi return incorrect sequence_index as response
      // and there is really now good solution for it, except restart dongle session
      // till the moment error gone.
      // https://github.com/obsidiansystems/ledger-app-tezos/blob/191-troubleshooting/README.md#error-unexpected-sequence-number-expected-0-got-191-on-macos
      // (solution in link above is not working, given here for reference)
      // Also it's a general issue for OSX really, but let's left it for all system, just in case
      // And let's have 10 times threshold to avoid trying infinitively
      static int maxAttempts = 10;
      if (e.what() == kHidapiSequence191 && maxAttempts > 0) {
         --maxAttempts;
         ScopedGuard guard([] {
            ++maxAttempts;
            });

         releaseDevice();
         initDevice();
         output.clear();

         return exchangeData(input, output, std::move(logHeader));
      }
      else {
         throw e;
      }
   }
}

BIP32_Node GetPubKeyHandler::getPublicKeyApdu(const bs::hd::Path& derivationPath
   , const std::unique_ptr<BIP32_Node>& parent)
{
   QByteArray payload;
   payload.append(derivationPath.length());
   for (auto key : derivationPath) {
      writeUintBE(payload, key);
   }

   QByteArray command = getApduHeader(Ledger::CLA, Ledger::INS_GET_WALLET_PUBLIC_KEY, 0, 0);
   command.append(static_cast<char>(payload.size()));
   command.append(payload);

   QByteArray response;
   if (!exchangeData(command, response, "[GetPubKeyHandler::getPublicKeyApdu] ")) {
      return {};
   }

   LedgerPublicKey pubKey;
   bool result = pubKey.parseFromResponse(response);

   auto data = SecureBinaryData::fromString(pubKey.pubKey.toStdString());
   Armory::Assets::Asset_PublicKey pubKeyAsset(data);
   SecureBinaryData chainCode = SecureBinaryData::fromString(pubKey.chainCode.toStdString());

   uint32_t fingerprint = 0;
   if (parent) {
      auto pubkey_hash = BtcUtils::getHash160(parent->getPublicKey());
      fingerprint = static_cast<uint32_t>(
         static_cast<uint32_t>(pubkey_hash[0] << 24) | static_cast<uint32_t>(pubkey_hash[1] << 16)
         | static_cast<uint32_t>(pubkey_hash[2] << 8) | static_cast<uint32_t>(pubkey_hash[3])
         );
   }

   BIP32_Node pubNode;
   pubNode.initFromPublicKey(derivationPath.length(), derivationPath.get(-1),
      fingerprint, pubKeyAsset.getCompressedKey(), chainCode);

   return pubNode;
}

std::shared_ptr<PubKeyOut> GetPubKeyHandler::processData(const std::shared_ptr<PubKeyIn>& in)
{
   if (!in) {
      return nullptr;
   }
   auto result = std::make_shared<PubKeyOut>();
   const std::lock_guard<std::mutex> lock(*hidLock_);
   if (!initDevice()) {
      logger_->info("[GetPubKeyHandler::processData] cannot init device");
      //emit error(Ledger::NO_DEVICE);
      return result;
   }
   for (const auto& path : in->paths) {
      result->pubKeys.push_back(retrievePublicKeyFromPath(path));
   }
   releaseDevice();
   return result;
}

BIP32_Node GetPubKeyHandler::retrievePublicKeyFromPath(const bs::hd::Path& derivationPath)
{
   std::unique_ptr<BIP32_Node> parent;
   if (derivationPath.length() > 1) {
      auto parentPath = derivationPath;
      parentPath.pop();
      parent = std::make_unique<BIP32_Node>(getPublicKeyApdu(parentPath, {}));
   }
   return getPublicKeyApdu(derivationPath, parent);
}

void LedgerDevice::getPublicKeys()
{
   auto deviceKey = key();
   auto walletInfo = std::make_shared<bs::core::HwWalletInfo>();
   walletInfo->type = bs::wallet::HardwareEncKey::WalletType::Ledger;
   walletInfo->vendor = deviceKey.vendor;
   walletInfo->label = deviceKey.label;
   walletInfo->deviceId = {};

   logger_->debug("[LedgerDevice::getPublicKeys] start retrieving device keys");
   auto inData = std::make_shared<PubKeyIn>();
   inData->paths.push_back({ { bs::hd::hardFlag } });
   inData->paths.push_back(getDerivationPath(testNet_, bs::hd::Nested));
   inData->paths.push_back(getDerivationPath(testNet_, bs::hd::Native));
   inData->paths.push_back(getDerivationPath(testNet_, bs::hd::NonSegWit));

   const auto& cb = [this, walletInfo](const std::shared_ptr<bs::OutData>& data)
   {
      const auto& result = std::static_pointer_cast<PubKeyOut>(data);

      if (result->pubKeys.size() != 4) {
         logger_->error("[LedgerDevice::getPublicKeys] invalid amount of public "
            "keys: {}", result->pubKeys.size());
         operationFailed("invalid amount of public keys: " + std::to_string(result->pubKeys.size()));
         return;
      }
      auto pubKey = result->pubKeys.at(0);
      try {
         walletInfo->xpubRoot = pubKey.getBase58().toBinStr();
      }
      catch (const std::exception& e) {
         logger_->error("[LedgerDevice::getPublicKeys] cannot retrieve root xpub key: {}", e.what());
         handleError(Ledger::INTERNAL_ERROR);
         return;
      }

      pubKey = result->pubKeys.at(1);
      try {
         walletInfo->xpubNestedSegwit = pubKey.getBase58().toBinStr();
      }
      catch (const std::exception& e) {
         logger_->error("[LedgerDevice::getPublicKeys] cannot retrieve nested segwit xpub key: {}", e.what());
         handleError(Ledger::INTERNAL_ERROR);
         return;
      }

      pubKey = result->pubKeys.at(2);
      try {
         walletInfo->xpubNativeSegwit = pubKey.getBase58().toBinStr();
      }
      catch (const std::exception& e) {
         logger_->error("[LedgerDevice::getPublicKeys] cannot retrieve native segwit xpub key: {}", e.what());
         handleError(Ledger::INTERNAL_ERROR);
         return;
      }

      pubKey = result->pubKeys.at(3);
      try {
         walletInfo->xpubLegacy = pubKey.getBase58().toBinStr();
      }
      catch (const std::exception& e) {
         logger_->error("[LedgerDevice::getPublicKeys] cannot retrieve legacy xpub key: {}", e.what());
         handleError(Ledger::INTERNAL_ERROR);
         return;
      }

      const auto& isValid = [](const bs::core::HwWalletInfo& info)
      {
         return !info.xpubRoot.empty() && !info.xpubNestedSegwit.empty() &&
            !info.xpubNativeSegwit.empty() && !info.xpubLegacy.empty();
      };
      if (!isValid(*walletInfo)) {
         logger_->error("[LedgerDevice::getPublicKeys] wallet info is invalid");
         handleError(Ledger::INTERNAL_ERROR);
         return;
      }
      else {
         logger_->debug("[LedgerDevice::getPublicKeys] operation succeeded");
      }
      cb_->walletInfoReady(key(), *walletInfo);
   };
   processQueued(inData, cb);
}

void LedgerDevice::signTX(const bs::core::wallet::TXSignRequest& coreReq)
{
   // retrieve inputs paths
   std::vector<bs::hd::Path> inputPaths;
   for (int i = 0; i < coreReq.armorySigner_.getTxInCount(); ++i) {
      auto spender = coreReq.armorySigner_.getSpender(i);
      const auto& bip32Paths = spender->getBip32Paths();
      if (bip32Paths.size() != 1) {
         throw std::logic_error("spender should only have one bip32 path");
      }
      auto pathFromRoot = bip32Paths.begin()->second.getDerivationPathFromSeed();

      bs::hd::Path path;
      for (unsigned i=0; i<pathFromRoot.size(); i++) {
         path.append(pathFromRoot[i]);
      }

      inputPaths.push_back(std::move(path));
   }

   // retrieve change path if any
   bs::hd::Path changePath;
   if (coreReq.change.value > 0) {
      const auto purp = bs::hd::purpose(coreReq.change.address.getType());
      if (coreReq.change.index.empty()) {
         throw std::logic_error(fmt::format("can't find change address index for '{}'", coreReq.change.address.display()));
      }

      changePath = getDerivationPath(testNet_, purp);
      changePath.append(bs::hd::Path::fromString(coreReq.change.index));
   }

   auto inPubData = std::make_shared<PubKeyIn>();
   inPubData->paths = inputPaths;

   const auto& cb = [this](const std::shared_ptr<bs::OutData>& data)
   {
      const auto& reply = std::static_pointer_cast<SignTXOut>(data);
      if (!reply) {
         logger_->error("[LedgerDevice::signTX] invalid callback data");
         operationFailed("invalid data");
         return;
      }
      cb_->txSigned(key(), reply->serInputSigs);
   };
   auto inData = std::make_shared<SignTXIn>();
   inData->key = key();
   inData->txReq = coreReq;
   inData->inputPaths = std::move(inputPaths);
   inData->changePath = std::move(changePath);

   const auto& cbPub = [this, inData, cb](const std::shared_ptr<bs::OutData>& data)
   {
      const auto& pubResult = std::static_pointer_cast<PubKeyOut>(data);
      inData->inputNodes = std::move(pubResult->pubKeys);
      processQueued(inData, cb);
   };
   processQueued(inPubData, cbPub);
}

void bs::hww::LedgerDevice::retrieveXPubRoot()
{
   auto deviceKey = key();
   auto walletInfo = std::make_shared<bs::core::HwWalletInfo>();
   walletInfo->type = bs::wallet::HardwareEncKey::WalletType::Ledger;
   walletInfo->vendor = deviceKey.vendor;
   walletInfo->label = deviceKey.label;
   walletInfo->deviceId = {};

   auto inData = std::make_shared<PubKeyIn>();
   inData->paths.push_back({ { bs::hd::hardFlag } });

   const auto& cb = [this, walletInfo](const std::shared_ptr<bs::OutData>& data)
   {
      const auto& result = std::static_pointer_cast<PubKeyOut>(data);

      if (result->pubKeys.size() != 1) {
         logger_->error("[LedgerDevice::retrieveXPubRoot] invalid amount of public "
            "keys: {}", result->pubKeys.size());
         handleError(Ledger::NO_INPUTDATA);
         return;
      }
      const auto& pubKey = result->pubKeys.at(0);
      if (xpubRoot_.empty()) {
         xpubRoot_ = pubKey.getPublicKey();
         const auto& devKey = key();
         cb_->publicKeyReady(devKey.id, devKey.walletId);
         return;
      }
      try {
         walletInfo->xpubRoot = pubKey.getBase58().toBinStr();
      }
      catch (const std::exception& e) {
         logger_->error("[LedgerDevice::retrieveXPubRoot] cannot retrieve root xpub key: {}", e.what());
         handleError(Ledger::INTERNAL_ERROR);
         return;
      }
      //TODO: invoke callback with walletInfo
   };
   processQueued(inData, cb);
}

void bs::hww::LedgerDevice::requestForRescan()
{
}

std::shared_ptr<bs::Worker> bs::hww::LedgerDevice::worker(const std::shared_ptr<InData>&)
{
   const std::vector<std::shared_ptr<Handler>> handlers{
      std::make_shared<GetPubKeyHandler>(logger_, hidDeviceInfo_, hidLock_)
      , std::make_shared<SignTXHandler>(logger_, hidDeviceInfo_, hidLock_)
   };
   return std::make_shared<bs::WorkerImpl>(handlers);
}

void LedgerDevice::handleError(int32_t errorCode)
{
   std::string error;
   switch (errorCode)
   {
   case Ledger::SW_NO_ENVIRONMENT:
      requestForRescan();
      //error = InfoStatus::kErrorNoEnvironment;
      break;
   case Ledger::SW_CANCELED_BY_USER:
      cancelledOnDevice();
      //error = HWInfoStatus::kCancelledByUser;
      break;
   case Ledger::NO_DEVICE:
      requestForRescan();
      error = HWInfoStatus::kErrorNoDevice.toStdString();
      break;
   case Ledger::SW_RECONNECT_DEVICE:
      requestForRescan();
      error = HWInfoStatus::kErrorReconnectDevice.toStdString();
      break;
   case Ledger::NO_INPUTDATA:
   default:
      error = HWInfoStatus::kErrorInternalError.toStdString();
      break;
   }
   lastError_ = error;
   operationFailed(error);
}

#if 0 // DO NOT DELETE JUST FOR HISTORICAL REFFERENCE
QByteArray getTrustedInputSegWit(const UTXO& utxo)
{
   QByteArray trustedInput;
   trustedInput.push_back(QByteArray::fromStdString(utxo.getTxHash().toBinStr()));
   writeUintLE(trustedInput, utxo.getTxOutIndex());
   writeUintLE(trustedInput, utxo.getValue());

   return trustedInput;
}
#endif   //0

void SignTXHandler::startUntrustedTransaction(const std::vector<QByteArray>& trustedInputs,
   const std::vector<QByteArray>& redeemScripts,
   unsigned txOutIndex, bool isNew, bool isSW, bool isRbf)
{
   {
      //setup untrusted transaction
      logger_->debug("[SignTXHandler::startUntrustedTransaction] start Init section");
      QByteArray initPayload;
      writeUintLE(initPayload, Ledger::DEFAULT_VERSION);
      writeVarInt(initPayload, trustedInputs.size());

      auto p2 = isNew ? 0x00 : 0x80;
      if (isSW) {
         p2 = isNew ? 0x02 : 0x80;
      }
      auto initCommand = getApduCommand(Ledger::CLA, Ledger::INS_HASH_INPUT_START
         , 0x00, p2, std::move(initPayload));

      QByteArray responseInit;
      if (!exchangeData(initCommand, responseInit, "[SignTXHandler::startUntrustedTransaction] InitPayload")) {
         releaseDevice();
         throw std::runtime_error("failed to init untrusted tx");
      }
      logger_->debug("[SignTXHandler::startUntrustedTransaction] done Init section");
   }

   //pass each input
   logger_->debug("[SignTXHandler::startUntrustedTransaction] start Input section");
   std::vector<QByteArray> inputCommands;
   for (unsigned i=0; i<trustedInputs.size(); i++) {
      const auto& trustedInput = trustedInputs[i];

      QByteArray inputPayload;

      /*assuming the entire payload will be less than 256 bytes*/

      inputPayload.push_back(0x01); //trusted input flag
      inputPayload.push_back(uint8_t(trustedInput.size()));
      inputPayload.push_back(trustedInput);

      const bool includeScript = (txOutIndex == std::numeric_limits<unsigned>::max() || txOutIndex == i);

      writeVarInt(inputPayload, includeScript ? redeemScripts[i].size() : 0);
      auto firstPart = getApduCommand(Ledger::CLA, Ledger::INS_HASH_INPUT_START, 0x80, 0x00, std::move(inputPayload));
      inputCommands.push_back(std::move(firstPart));
      //utxo script
      inputPayload.clear();
      if (includeScript) {
         inputPayload.push_back(redeemScripts[i]);
      }

      //sequence
      uint32_t defSeq = isRbf ? Ledger::DEFAULT_SEQUENCE - 2 : Ledger::DEFAULT_SEQUENCE;
      writeUintLE(inputPayload, defSeq);

      auto secondPart = getApduCommand(Ledger::CLA, Ledger::INS_HASH_INPUT_START, 0x80, 0x00, std::move(inputPayload));
      inputCommands.push_back(std::move(secondPart));
   }

   for (const auto &inputCommand : inputCommands) {
      QByteArray responseInput;
      if (!exchangeData(inputCommand, responseInput, "[SignTXHandler::startUntrustedTransaction] inputPayload")) {
         releaseDevice();
         throw std::runtime_error("failed to create untrusted tx");
      }
   }
   logger_->debug("[SignTXHandler::startUntrustedTransaction] done Input section");
}

void SignTXHandler::finalizeInputFull(const std::shared_ptr<SignTXIn>& inData)
{
   if (inData->changePath.length() != 0) {
      logger_->debug("[SignTXHandler::finalizeInputFull] start Change section");
      // If tx has change, we send derivation path of return address in prio
      // before send all output addresses and change with it, so device could detect it
      QByteArray changeOutputPayload;
      changeOutputPayload.append(static_cast<char>(inData->changePath.length()));


      for (const auto& el : inData->changePath) {
         writeUintBE(changeOutputPayload, el);
      }

      auto changeCommand = getApduCommand(Ledger::CLA, Ledger::INS_HASH_INPUT_FINALIZE_FULL
         , 0xFF, 0x00, std::move(changeOutputPayload));
      QByteArray responseInput;
      if (!exchangeData(changeCommand, responseInput, "[SignTXHandler::finalizeInputFull] changePayload ")) {
         releaseDevice();
         return;
      }
      logger_->debug("[SignTXHandler::finalizeInputFull] done Change section");
   }

   logger_->debug("[SignTXHandler::finalizeInputFull] start output section");
   size_t totalOutput = inData->txReq.armorySigner_.getTxOutCount();

   QByteArray outputFullPayload;
   writeVarInt(outputFullPayload, totalOutput);

   for (unsigned i=0; i < inData->txReq.armorySigner_.getTxOutCount(); i++) {
      auto recipient = inData->txReq.armorySigner_.getRecipient(i);
      outputFullPayload.push_back(QByteArray::fromStdString(recipient->getSerializedScript().toBinStr()));
   }

   QVector<QByteArray> outputCommands;
   for (int proccessed = 0; proccessed < outputFullPayload.size(); proccessed += Ledger::OUT_CHUNK_SIZE) {
      uint8_t p1 = (proccessed + Ledger::OUT_CHUNK_SIZE > outputFullPayload.size()) ? 0x80 : 0x00;

      auto chunk = outputFullPayload.mid(proccessed, Ledger::OUT_CHUNK_SIZE);
      auto outputCommand = getApduCommand(Ledger::CLA, Ledger::INS_HASH_INPUT_FINALIZE_FULL,
         p1, 0x00, std::move(chunk));
      outputCommands.push_back(std::move(outputCommand));
   }

   //emit info(HWInfoStatus::kPressButton);

   for (auto &outputCommand : outputCommands) {
      QByteArray responseOutput;
      if (!exchangeData(outputCommand, responseOutput, "[SignTXHandler::finalizeInputFull] outputPayload")) {
         releaseDevice();
         throw std::runtime_error("failed to upload recipients");
      }
   }
   logger_->debug("[SignTXHandler::finalizeInputFull] done output section");
}

void SignTXHandler::processTXLegacy(const std::shared_ptr<SignTXIn>& inData
   , const std::shared_ptr<SignTXOut>& outData)
{
   //upload supporting tx to ledger, get trusted input back for our outpoints
   //emit info(HWInfoStatus::kTransaction);
   std::vector<QByteArray> trustedInputs;
   for (unsigned i=0; i < inData->txReq.armorySigner_.getTxInCount(); i++) {
      auto spender = inData->txReq.armorySigner_.getSpender(i);
      auto trustedInput = getTrustedInput(inData, spender->getOutputHash()
         , spender->getOutputIndex());
      trustedInputs.push_back(trustedInput);
   }

   // -- collect all redeem scripts
   std::vector<QByteArray> redeemScripts;
   for (int i = 0; i < inData->txReq.armorySigner_.getTxInCount(); ++i) {
      auto& utxo = inData->txReq.armorySigner_.getSpender(i)->getUtxo();
      auto redeemScript = utxo.getScript();
      auto redeemScriptQ = QByteArray(redeemScript.getCharPtr(), redeemScript.getSize());
      redeemScripts.push_back(redeemScriptQ);
   }

   // -- Start input upload section --

   //upload the redeem script for each outpoint
   if (inData->inputPaths.size() != inData->txReq.armorySigner_.getTxInCount()) {
      //TODO: report error
      return;
   }
   std::map<int, QByteArray> responseSigned;
   for (int i = 0; i < inData->txReq.armorySigner_.getTxInCount(); ++i) {
      //pass true as this is a newly presented redeem script
      startUntrustedTransaction(trustedInputs, redeemScripts, i, i == 0, false
         , inData->txReq.RBF);
      // -- Done input section --

      // -- Start output section --
      //upload our recipients as serialized outputs
      finalizeInputFull(inData);

      // -- Done output section --

      // At this point user verified all outputs and we can start signing inputs
      //emit info(HWInfoStatus::kReceiveSignedTx);

      // -- Start signing one by one all addresses --
      logger_->debug("[LedgerCommandThread] processTXLegacy - Start signing section");

      auto &path = inData->inputPaths.at(i);
      QByteArray signPayload;
      signPayload.append(static_cast<char>(path.length()));

      QByteArray derivationPath;
      for (auto el : path) {
         writeUintBE(derivationPath, el);
      }

      signPayload.append(derivationPath);
      signPayload.append(static_cast<char>(0x00));
      writeUintBE(signPayload, static_cast<uint32_t>(0));
      signPayload.append(static_cast<char>(0x01));

      auto commandSign = getApduCommand(Ledger::CLA, Ledger::INS_HASH_SIGN, 0x00, 0x00, std::move(signPayload));

      QByteArray responseInputSign;
      if (!exchangeData(commandSign, responseInputSign, "[LedgerCommandThread] signTX - Sign Payload")) {
         releaseDevice();
         return;
      }
      responseInputSign[0] = 0x30; // force first but to be 0x30 for a newer version of ledger
      responseSigned[i] = responseInputSign;
   }

   logger_->debug("[LedgerCommandThread] processTXLegacy - Done signing section");
   // -- Done signing one by one all addresses --

   // Done with device in this point

   // Composing and send data back
   //emit info(HWInfoStatus::kTransactionFinished);
   // Debug check
   debugPrintLegacyResult(inData, responseSigned.at(0), inData->inputNodes[0]);

   sendTxSigningResult(outData, responseSigned);
}

void SignTXHandler::processTXSegwit(const std::shared_ptr<SignTXIn>& inData
   , const std::shared_ptr<SignTXOut>& outData)
{
   //emit info(HWInfoStatus::kTransaction);
   // ---- Prepare all pib32 nodes for input from device -----
   auto segwitData = getSegwitData(inData);

   //upload supporting tx to ledger, get trusted input back for our outpoints
   std::vector<QByteArray> trustedInputs;
   for (unsigned i=0; i < inData->txReq.armorySigner_.getTxInCount(); i++) {
      auto spender = inData->txReq.armorySigner_.getSpender(i);
      auto trustedInput = getTrustedInput(inData, spender->getOutputHash()
         , spender->getOutputIndex());
      trustedInputs.push_back(trustedInput);
   }

   // -- Collect all redeem scripts

   std::vector<QByteArray> redeemScripts;
   for (int i = 0; i < inData->txReq.armorySigner_.getTxInCount(); ++i) {
      const auto& path = inData->inputPaths.at(i);

      BinaryData redeemScriptWitness;
      if (isNativeSegwit(path)) {
         const auto& utxo = inData->txReq.armorySigner_.getSpender(i)->getUtxo();
         auto redeemScript = utxo.getScript();
         redeemScriptWitness = BtcUtils::getP2WPKHWitnessScript(redeemScript.getSliceRef(2, 20));
      }
      else if (isNestedSegwit(path)) {
         redeemScriptWitness = segwitData.redeemScripts.at(i);
      }
      redeemScripts.push_back(QByteArray(redeemScriptWitness.toBinStr().c_str()));
   }

   // -- Start input upload section --
   {
      startUntrustedTransaction(trustedInputs, redeemScripts,
         std::numeric_limits<unsigned>::max(), true, true, inData->txReq.RBF);
   }
   // -- Done input section --

   // -- Start output section --
   {
      //upload our recipients as serialized outputs
      finalizeInputFull(inData);
   }
   // -- Done output section --

   // At this point user verified all outputs and we can start signing inputs
   //emit info(HWInfoStatus::kReceiveSignedTx);

   // -- Start signing one by one all addresses --
   std::map<int, QByteArray> responseSigned;
   for (int i = 0; i < inData->txReq.armorySigner_.getTxInCount(); ++i) {
      const auto& path = inData->inputPaths.at(i);

      startUntrustedTransaction({ trustedInputs.at(i) }, { redeemScripts.at(i) }
         , 0, false, true, inData->txReq.RBF);

      QByteArray signPayload;
      signPayload.append(static_cast<char>(path.length()));

      QByteArray derivationPath;
      for (auto el : path) {
         writeUintBE(derivationPath, el);
      }

      signPayload.append(derivationPath);
      signPayload.append(static_cast<char>(0x00));
      writeUintBE(signPayload, static_cast<uint32_t>(0));
      signPayload.append(static_cast<char>(0x01));

      auto commandSign = getApduCommand(Ledger::CLA, Ledger::INS_HASH_SIGN, 0x00, 0x00, std::move(signPayload));
      QByteArray responseInputSign;
      if (!exchangeData(commandSign, responseInputSign, "[SignTXHandler::processTXSegwit] sign Payload")) {
         releaseDevice();
         return;
      }
      responseInputSign[0] = 0x30; // force first but to be 0x30 for a newer version of ledger
      responseSigned[i] = responseInputSign;
   }

   //emit info(HWInfoStatus::kTransactionFinished);

   // -- Done signing one by one all addresses --
   sendTxSigningResult(outData, responseSigned);
}

SegwitInputData SignTXHandler::getSegwitData(const std::shared_ptr<SignTXIn>& inData)
{
   logger_->info("[SignTXHandler::getSegwitData] start retrieving segwit data");
   SegwitInputData data;
   for (unsigned i = 0; i < inData->txReq.armorySigner_.getTxInCount(); i++) {
      const auto& path = inData->inputPaths.at(i);
      auto spender = inData->txReq.armorySigner_.getSpender(i);

      if (!isNestedSegwit(path)) {
         continue;
      }
      const auto& pubKeyNode = inData->inputNodes.at(i);

      //recreate the p2wpkh & witness scripts
      auto compressedKey = CryptoECDSA().CompressPoint(pubKeyNode.getPublicKey());
      auto pubKeyHash = BtcUtils::getHash160(compressedKey);

      auto witnessScript = BtcUtils::getP2WPKHWitnessScript(pubKeyHash);

      BinaryWriter bwSwScript;
      bwSwScript.put_uint8_t(16);
      bwSwScript.put_BinaryData(BtcUtils::getP2WPKHOutputScript(pubKeyHash));
      const auto& swScript = bwSwScript.getData();

      /* sanity check: make sure the swScript is the preimage to the utxo's p2sh script */

      //recreate p2sh hash
      //auto p2shHash = BtcUtils::hash160(swScript);
      auto p2shHash = BtcUtils::hash160(swScript.getSliceRef(1, 22));

      //recreate p2sh script
      auto p2shScript = BtcUtils::getP2SHScript(p2shHash);

      //check vs utxo's script
      if (spender->getOutputScript() != p2shScript) {
         throw std::runtime_error("p2sh script mismatch");
      }

      data.preimages[i] = swScript;
      data.redeemScripts[i] = std::move(witnessScript);
   }
   logger_->info("[SignTXHandler::getSegwitData] done retrieving segwit data");
   return data;
}

void SignTXHandler::sendTxSigningResult(const std::shared_ptr<SignTXOut>& outData
   , const std::map<int, QByteArray>& responseSigned)
{
   BinaryWriter bw;
   bw.put_var_int(responseSigned.size());
   for (const auto& signedInput : responseSigned) {
      bw.put_uint32_t(signedInput.first);
      bw.put_var_int(signedInput.second.size());
      bw.put_BinaryData(BinaryData::fromString(signedInput.second.toStdString()));
   }
   outData->serInputSigs = bw.getData();
}

void SignTXHandler::debugPrintLegacyResult(const std::shared_ptr<SignTXIn>& inData
   , const QByteArray& responseSigned, const BIP32_Node& node)
{
   BinaryWriter bw;
   bw.put_uint32_t(1); //version
   bw.put_var_int(1); //txin count

   //inputs
   const auto& utxo = inData->txReq.armorySigner_.getSpender(0)->getUtxo();

   //outpoint
   bw.put_BinaryData(utxo.getTxHash());
   bw.put_uint32_t(utxo.getTxOutIndex());

   //create sigscript
   BinaryWriter bwSigScript;
   BinaryData bdScript((const uint8_t*)responseSigned.data(), responseSigned.size());
   bwSigScript.put_var_int(responseSigned.size()); //sig size
   bwSigScript.put_BinaryData(bdScript); //sig 

   //compressed pubkey
   auto pubKey = node.getPublicKey();
   auto compressedKey = CryptoECDSA().CompressPoint(pubKey);
   bwSigScript.put_uint8_t(33); //pubkey size
   bwSigScript.put_BinaryDataRef(compressedKey); //pubkey

   //put sigscript
   bw.put_var_int(bwSigScript.getSize());
   bw.put_BinaryData(bwSigScript.getData());

   //sequence
   bw.put_uint32_t(inData->txReq.RBF ? Ledger::DEFAULT_SEQUENCE - 2 : Ledger::DEFAULT_SEQUENCE);

   //txouts
   bw.put_var_int(1); //count

   bw.put_BinaryData(inData->txReq.armorySigner_.getRecipient(0)->getSerializedScript());
   bw.put_uint32_t(0);

   logger_->debug("[LedgerDevice::SignTXHandler] legacy result: {}", bw.getData().toHexStr());
}

std::shared_ptr<SignTXOut> SignTXHandler::processData(const std::shared_ptr<SignTXIn>& inData)
{
   auto result = std::make_shared<SignTXOut>();
   if (!initDevice()) {
      logger_->info("[SignTXHandler::processData] cannot init device");
      //emit error(Ledger::NO_DEVICE);
      return result;
   }
   if (!inData || !inData->txReq.isValid() || inData->inputPaths.empty()) {
      logger_->error("[SignTXHandler::processData] invalid request");
      //emit error(Ledger::NO_INPUTDATA);
      return result;
   }
   if (isNonSegwit(inData->inputPaths.at(0))) {
      processTXLegacy(inData, result);
   }
   else {
      processTXSegwit(inData, result);
   }
   releaseDevice();
   return result;
}

QByteArray SignTXHandler::getTrustedInput(const std::shared_ptr<SignTXIn>& inData
   , const BinaryData& hash, unsigned txOutId)
{
   logger_->debug("[SignTXHandler::getTrustedInput] start retrieve trusted input for legacy address");

   //find the supporting tx
   auto tx = inData->txReq.armorySigner_.getSupportingTx(hash);
   std::vector<QByteArray> inputCommands;

   {
      //trusted input request header
      QByteArray txPayload;
      writeUintBE(txPayload, txOutId); //outpoint index

      writeUintLE(txPayload, tx.getVersion()); //supporting tx version
      writeVarInt(txPayload, tx.getNumTxIn()); //supporting tx input count
      auto command = getApduCommand(
         Ledger::CLA, Ledger::INS_GET_TRUSTED_INPUT, 0x00, 0x00, std::move(txPayload));
      inputCommands.push_back(std::move(command));
   }

   //supporting tx inputs
   for (unsigned i = 0; i < tx.getNumTxIn(); i++) {
      auto txIn = tx.getTxInCopy(i);
      auto outpoint = txIn.getOutPoint();
      auto outpointRaw = outpoint.serialize();
      auto scriptSig = txIn.getScriptRef();

      //36 bytes of outpoint
      QByteArray txInPayload;
      txInPayload.push_back(QByteArray::fromRawData(
         outpointRaw.getCharPtr(), outpointRaw.getSize()));

      //txin scriptSig size as varint
      writeVarInt(txInPayload, scriptSig.getSize());

      auto commandInput = getApduCommand(
         Ledger::CLA, Ledger::INS_GET_TRUSTED_INPUT, 0x80, 0x00, std::move(txInPayload));
      inputCommands.push_back(std::move(commandInput));

      //txin scriptSig, assuming it's less than 251 bytes for the sake of simplicity
      QByteArray txInScriptSig;
      txInScriptSig.push_back(QByteArray::fromRawData(
         scriptSig.toCharPtr(), scriptSig.getSize()));
      writeUintLE(txInScriptSig, txIn.getSequence()); //sequence

      auto commandScriptSig = getApduCommand(
         Ledger::CLA, Ledger::INS_GET_TRUSTED_INPUT, 0x80, 0x00, std::move(txInScriptSig));
      inputCommands.push_back(std::move(commandScriptSig));
   }

   {
      //number of outputs
      QByteArray txPayload;
      writeVarInt(txPayload, tx.getNumTxOut()); //supporting tx input count
      auto command = getApduCommand(
         Ledger::CLA, Ledger::INS_GET_TRUSTED_INPUT, 0x80, 0x00, std::move(txPayload));
      inputCommands.push_back(std::move(command));
   }

   //supporting tx outputs
   for (unsigned i = 0; i < tx.getNumTxOut(); i++) {
      auto txout = tx.getTxOutCopy(i);
      auto script = txout.getScriptRef();

      QByteArray txOutput;
      writeUintLE(txOutput, txout.getValue()); //txout value
      writeVarInt(txOutput, script.getSize()); //txout script len

      auto commandTxOut = getApduCommand(
         Ledger::CLA, Ledger::INS_GET_TRUSTED_INPUT, 0x80, 0x00, std::move(txOutput));
      inputCommands.push_back(std::move(commandTxOut));

      //again, assuming the txout script is shorter than 255 bytes
      QByteArray txOutScript;
      txOutScript.push_back(QByteArray::fromRawData(
         script.toCharPtr(), script.getSize()));

      auto commandScript = getApduCommand(
         Ledger::CLA, Ledger::INS_GET_TRUSTED_INPUT, 0x80, 0x00, std::move(txOutScript));
      inputCommands.push_back(std::move(commandScript));
   }

   for (auto& inputCommand : inputCommands) {
      QByteArray responseInput;
      if (!exchangeData(inputCommand, responseInput, "[LedgerCommandThread] signTX - getting trusted input")) {
         releaseDevice();
         throw std::runtime_error("failed to get trusted input");
      }
   }

   //locktime
   QByteArray locktime;
   writeUintLE(locktime, tx.getLockTime());
   auto command = getApduCommand(
      Ledger::CLA, Ledger::INS_GET_TRUSTED_INPUT, 0x80, 0x00, std::move(locktime));

   QByteArray trustedInput;
   if (!exchangeData(command, trustedInput, "[SignTXHandler::getTrustedInput] ")) {
      releaseDevice();
      throw std::runtime_error("failed to get trusted input");
   }

   logger_->debug("[SignTXHandler::getTrustedInput] done retrieve trusted input for legacy address");
   return trustedInput;
}
