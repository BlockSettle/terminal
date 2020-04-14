/*

***********************************************************************************
* Copyright (C) 2016 - , BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "spdlog/logger.h"
#include "ledger/ledgerDevice.h"
#include "ledger/ledgerClient.h"
#include "Assets.h"
#include "ProtobufHeadlessUtils.h"
#include "CoreWallet.h"
#include "Wallets/SyncWalletsManager.h"
#include "Wallets/SyncHDWallet.h"

#include "QByteArray"
#include "QDataStream"

namespace {
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
      uint16_t chunkNumber = 0;

      unsigned char buf[Ledger::CHUNK_MAX_BLOCK];
      uint16_t result = hid_read(dongle, buf, Ledger::CHUNK_MAX_BLOCK);
      if (result < 0) {
         return result;
      }

      std::vector<uint8_t> buff(&buf[0], &buf[0] + 64);

      QByteArray chunk(reinterpret_cast<char*>(buf), Ledger::CHUNK_MAX_BLOCK);
      assert(chunkNumber++ == chunk.mid(3, 2).toHex().toInt());

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
         assert(chunkNumber++ == chunk.mid(3, 2).toHex().toInt());

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

LedgerDevice::LedgerDevice(HidDeviceInfo&& hidDeviceInfo, bool testNet,
   std::shared_ptr<bs::sync::WalletsManager> walletManager, const std::shared_ptr<spdlog::logger> &logger, QObject* parent /*= nullptr*/)
   : HwDeviceInterface(parent)
   , hidDeviceInfo_(std::move(hidDeviceInfo))
   , logger_(logger)
   , testNet_(testNet)
   , walletManager_(walletManager)
{
}

LedgerDevice::~LedgerDevice()
{
   //releaseDevice();
}

DeviceKey LedgerDevice::key() const
{
   return {
      hidDeviceInfo_.productString_,
      hidDeviceInfo_.manufacturerString_,
      hidDeviceInfo_.manufacturerString_,
      {},
      {},
      DeviceType::HWLedger
   };
}

DeviceType LedgerDevice::type() const
{
   return DeviceType::HWLedger;
}

void LedgerDevice::init(AsyncCallBack&& cb /*= nullptr*/)
{
   if (cb) {
      cb();
   }
   // Define when async
}

void LedgerDevice::cancel()
{
   // Define when async
}

void LedgerDevice::getPublicKey(AsyncCallBackCall&& cb /*= nullptr*/)
{
   auto *commandThread = blankCommand(std::move(cb));
   commandThread->prepareGetPublicKey(key());
   connect(commandThread, &LedgerCommandThread::finished, commandThread, &QObject::deleteLater);
   commandThread->start();
}

void LedgerDevice::signTX(const QVariant& reqTX, AsyncCallBackCall&& cb /*= nullptr*/)
{
   Blocksettle::Communication::headless::SignTxRequest request;
   if (!request.ParseFromString(reqTX.toByteArray().toStdString())) {
      logger_->debug("[LedgerDevice] signTX - failed to parse transaction request ");
      emit operationFailed();
      return;
   }

   auto coreReq = bs::signer::pbTxRequestToCore(request);

   std::vector<bs::hd::Path> inputPathes;
   for (auto &utxo : coreReq.inputs) {
      const auto address = bs::Address::fromUTXO(utxo);
      const auto purp = bs::hd::purpose(address.getType());

      std::string addrIndex;
      for (const auto &walletId : coreReq.walletIds) {
         auto wallet = walletManager_->getWalletById(walletId);
         addrIndex = wallet->getAddressIndex(address);
         if (!addrIndex.empty()) {
            break;
         }
      }

      auto path = getDerivationPath(testNet_, purp);
      path.append(bs::hd::Path::fromString(addrIndex));
      inputPathes.push_back(std::move(path));
   }

   bs::hd::Path changePath;
   if (coreReq.change.value > 0) {
      const bool isNestedSegwit = (coreReq.change.address.getType() == AddressEntryType_P2SH);
      const auto purp = bs::hd::purpose(coreReq.change.address.getType());
      if (coreReq.change.index.empty()) {
         throw std::logic_error(fmt::format("can't find change address index for '{}'", coreReq.change.address.display()));
      }

      changePath = getDerivationPath(testNet_, purp);
      changePath.append(bs::hd::Path::fromString(coreReq.change.index));
   }

   auto *commandThread = blankCommand(std::move(cb));
   commandThread->prepareSignTx(key(), std::move(coreReq), std::move(inputPathes), std::move(changePath));
   commandThread->start();
}

LedgerCommandThread *LedgerDevice::blankCommand(AsyncCallBackCall&& cb /*= nullptr*/)
{
   auto newCommandThread = new LedgerCommandThread(hidDeviceInfo_, testNet_, logger_, this);
   connect(newCommandThread, &LedgerCommandThread::resultReady, this, [cbCopy = std::move(cb)](QVariant result) {
      if (cbCopy) {
         cbCopy(std::move(result));
      }
   });
   connect(newCommandThread, &LedgerCommandThread::error, this, [caller = QPointer<LedgerDevice>(this)]() {
      if (caller) {
         caller->operationFailed();
      }
   });
   connect(newCommandThread, &LedgerCommandThread::finished, newCommandThread, &QObject::deleteLater);

   return newCommandThread;
}

LedgerCommandThread::LedgerCommandThread(const HidDeviceInfo &hidDeviceInfo, bool testNet
   , const std::shared_ptr<spdlog::logger> &logger, QObject *parent /* = nullptr */)
   : QThread(parent)
   , hidDeviceInfo_(hidDeviceInfo)
   , testNet_(testNet)
   , logger_(logger)
{
}

LedgerCommandThread::~LedgerCommandThread()
{
   releaseDevice();
}

void LedgerCommandThread::run()
{
   switch (threadPurpose_)
   {
   case HardwareCommand::GetPublicKey:
      processGetPublicKey();
      break;
   case HardwareCommand::SignTX:
      processTXSigning();
      break;
   case HardwareCommand::None:
   default:
      // Please add handler for a new command
      assert(false);
      break;
   }
}

void LedgerCommandThread::prepareGetPublicKey(const DeviceKey &deviceKey)
{
   threadPurpose_ = HardwareCommand::GetPublicKey;
   deviceKey_ = deviceKey;
}

void LedgerCommandThread::prepareSignTx(const DeviceKey &deviceKey, bs::core::wallet::TXSignRequest&& coreReq
   , std::vector<bs::hd::Path>&& paths, bs::hd::Path&& changePath)
{
   threadPurpose_ = HardwareCommand::SignTX;
   deviceKey_ = deviceKey;
   coreReq_ = std::make_unique<bs::core::wallet::TXSignRequest>(std::move(coreReq));
   inputPaths_ = std::move(paths);
   changePath_ = std::move(changePath);
}

void LedgerCommandThread::processGetPublicKey()
{
   if (!initDevice()) {
      logger_->debug(
         "[LedgerCommandThread] getPublicKey - Cannot open device.");
      emit error();
      return;
   }

   auto deviceKey = deviceKey_;
   HwWalletWrapper walletInfo;
   walletInfo.info_.vendor_ = deviceKey.vendor_.toStdString();
   walletInfo.info_.label_ = deviceKey.deviceLabel_.toStdString();
   walletInfo.info_.deviceId_ = deviceKey.deviceId_.toStdString();

   auto pubKey = retrievePublicKeyFromPath({ { bs::hd::hardFlag } });
   try {
      walletInfo.info_.xpubRoot_ = pubKey.getBase58().toBinStr();
   }
   catch (...) {
      logger_->debug(
         "[LedgerCommandThread] getPublicKey - Cannot retrieve root xpub key.");
      emit error();
      return;
   }

   pubKey = retrievePublicKeyFromPath(getDerivationPath(testNet_, bs::hd::Nested));
   try {
      walletInfo.info_.xpubNestedSegwit_ = pubKey.getBase58().toBinStr();
   }
   catch (...) {
      logger_->debug(
         "[LedgerCommandThread] getPublicKey - Cannot retrieve nested segwit xpub key.");
      emit error();
      return;
   }

   pubKey = retrievePublicKeyFromPath(getDerivationPath(testNet_, bs::hd::Native));
   try {
      walletInfo.info_.xpubNativeSegwit_ = pubKey.getBase58().toBinStr();
   }
   catch (...) {
      logger_->debug(
         "[LedgerCommandThread] getPublicKey - Cannot retrieve native segwit xpub key.");
      emit error();
      return;
   }

   pubKey = retrievePublicKeyFromPath(getDerivationPath(testNet_, bs::hd::NonSegWit));
   try {
      walletInfo.info_.xpubLegacy_ = pubKey.getBase58().toBinStr();
   }
   catch (...) {
      logger_->debug(
         "[LedgerCommandThread] getPublicKey - Cannot retrieve legacy xpub key.");
      emit error();
      return;
   }

   if (!walletInfo.isValid()) {
      logger_->debug(
         "[LedgerCommandThread] getPublicKey - Wallet info is not correct.");
      emit error();
      return;
   }
   else {
      logger_->debug(
         "[LedgerCommandThread] getPublicKey - Operation succeeded.\nRoot xpub : "
         + walletInfo.info_.xpubRoot_ + " \nNested xpub: "
         + walletInfo.info_.xpubNestedSegwit_ + " \nNativeSegwit: "
         + walletInfo.info_.xpubNativeSegwit_);
   }

   emit resultReady(QVariant::fromValue<>(walletInfo));
}

BIP32_Node LedgerCommandThread::retrievePublicKeyFromPath(bs::hd::Path&& derivationPath)
{
   // Parent
   std::unique_ptr<BIP32_Node> parent = nullptr;
   if (derivationPath.length() > 1) {
      auto parentPath = derivationPath;
      parentPath.pop();
      parent.reset(new BIP32_Node(getPublicKeyApdu(std::move(parentPath))));
   }

   return getPublicKeyApdu(std::move(derivationPath), parent);
}

BIP32_Node LedgerCommandThread::getPublicKeyApdu(bs::hd::Path&& derivationPath, const std::unique_ptr<BIP32_Node>& parent)
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
   if (!exchangeData(command, response, "[LedgerCommandThread] getPublicKeyApdu - ")) {
      emit error();
      return {};
   }

   LedgerPublicKey pubKey;
   bool result = pubKey.parseFromResponse(response);

   auto data = SecureBinaryData::fromString(pubKey.pubKey_.toStdString());
   Asset_PublicKey pubKeyAsset(data);
   SecureBinaryData chainCode = SecureBinaryData::fromString(pubKey.chainCode_.toStdString());

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

void LedgerCommandThread::processTXSigning(/*const QVariant& reqTX*/)
{
   
   if (!initDevice()) {
      logger_->info(
         "[LedgerCommandThread] signTX - Cannot open device.");
      emit error();
      return;
   }

   if (!coreReq_) {
      logger_->debug("[LedgerCommandThread] signTX - the core request is no valid");
      emit error();
      return;
   }

   // ---- Prepare all pib32 nodes for input from device -----
   logger_->debug("[LedgerCommandThread] signTX - retrieving pubKey per input");

   std::vector<BIP32_Node> inputNodes;
   for (auto const &path : inputPaths_) {
      auto pubKeyNode = retrievePublicKeyFromPath(std::move(bs::hd::Path(path)));
      inputNodes.push_back(pubKeyNode);
   }

   // -- Start Init section --
   logger_->debug("[LedgerCommandThread] signTX - Start Init section");
   QByteArray initPayload;
   writeUintBE(initPayload, Ledger::DEFAULT_VERSION);
   writeVarInt(initPayload, coreReq_->inputs.size());
   auto initCommand = getApduCommand(Ledger::CLA, Ledger::INS_HASH_INPUT_START, 0x00, 0x02, std::move(initPayload));

   QByteArray responseInit;
   if (!exchangeData(initCommand, responseInit, "[LedgerCommandThread] signTX - InitPayload")) {
      releaseDevice();
      emit error();
      return;
   }
   // -- End Init section --

   // -- Start input section --

   logger_->debug("[LedgerCommandThread] signTX - Start Input section");
   QVector<QByteArray> inputCommands;
   QByteArray script;
   for (auto &utxo: coreReq_->inputs) {
      QByteArray inputPayload;
      inputPayload.append(Ledger::SEGWIT_TYPE);

      inputPayload.append(QByteArray::fromStdString(utxo.getTxHash().toBinStr()));
      writeUintLE(inputPayload, static_cast<uint32_t>(0));
      writeUintLE(inputPayload, utxo.getValue());
      writeVarInt(inputPayload, static_cast<uint32_t>(0));

      auto command = getApduCommand(Ledger::CLA, Ledger::INS_HASH_INPUT_START, 0x80, 0x00, std::move(inputPayload));
      inputCommands.push_back(std::move(command));

      inputPayload.clear();
      uint32_t defSeq = Ledger::DEFAULT_SEQUENCE;
      if (coreReq_->RBF) {
         defSeq -= 2;
      }
      writeUintLE(inputPayload, defSeq);

      command.clear();
      command = getApduCommand(Ledger::CLA, Ledger::INS_HASH_INPUT_START, 0x80, 0x00, std::move(inputPayload));
      inputCommands.push_back(std::move(command));
   }

   for (auto &inputCommand : inputCommands) {
      QByteArray responseInput;
      if (!exchangeData(inputCommand, responseInput, "[LedgerCommandThread] signTX - inputPayload")) {
         releaseDevice();
         emit error();
         return;
      }
   }

   // -- End input section --

   //// -- Start change section -- 
   //logger_->debug("[LedgerCommandThread] signTX - Start Change section");

   //if (changePath_.length() != 0) {
   //   QByteArray changeInputPayload;
   //   changeInputPayload.append(static_cast<char>(changePath_.length()));
   //   for (auto el : changePath_) {
   //      writeUintBE(changeInputPayload, el);
   //   }

   //   auto changeCommand = getApduCommand(Ledger::CLA, Ledger::INS_HASH_INPUT_FINALIZE_FULL, 0xFF, 0x00, std::move(changeInputPayload));

   //   QByteArray responseInput;
   //   if (!exchangeData(changeCommand, responseInput, "[LedgerCommandThread] signTX - change command")) {
   //      releaseDevice();
   //      emit error();
   //      return;
   //   }
   //}

   //// -- End Change section --

   // -- Start output section --
   logger_->debug("[LedgerCommandThread] signTX - Start Output section");

   QByteArray outputFullPayload;
   writeVarInt(outputFullPayload, coreReq_->recipients.size());
   for (auto &recipient : coreReq_->recipients) {
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

   for (auto &outputCommand : outputCommands) {
      QByteArray responseOutput;
      if (!exchangeData(outputCommand, responseOutput, "[LedgerCommandThread] signTX - outputPayload")) {
         releaseDevice();
         emit error();
         return;
      }
   }

   // -- End output section --

   // In this point user verified all outputs and we could start signing inputs

   QVector<QByteArray> inputSignCommands;
   //uint32_t sequenceId2 = 0;
   for (int i = 0; i < coreReq_->inputs.size(); ++i) {
      auto &utxo = coreReq_->inputs[i];

      QByteArray initSignPayload;
      writeUintBE(initSignPayload, Ledger::DEFAULT_VERSION);
      writeVarInt(initSignPayload, 0x01);
      auto initCommandSign = getApduCommand(Ledger::CLA, Ledger::INS_HASH_INPUT_START, 0x00, 0x80, std::move(initSignPayload));
      inputSignCommands.push_back(std::move(initCommandSign));


      QByteArray inputPayload;
      inputPayload.append(Ledger::SEGWIT_TYPE);
      inputPayload.append(QByteArray::fromStdString(utxo.getTxHash().toBinStr()));
      writeUintLE(inputPayload, static_cast<uint32_t>(0));
      writeUintLE(inputPayload, utxo.getValue());

      auto scriptHash = BtcUtils::getHash160(inputNodes[i].getPublicKey());
      script = QByteArray::fromStdString(BtcUtils::getP2WPKHWitnessScript(scriptHash).toBinStr());

      writeVarInt(inputPayload, static_cast<uint32_t>(script.size()));

      auto commandSign = getApduCommand(Ledger::CLA, Ledger::INS_HASH_INPUT_START, 0x80, 0x00, std::move(inputPayload));
      inputSignCommands.push_back(std::move(commandSign));

      inputPayload.clear();
      inputPayload.append(script);
      uint32_t defSeq = Ledger::DEFAULT_SEQUENCE;
      if (coreReq_->RBF) {
         defSeq -= 2;
      }
      writeUintLE(inputPayload, defSeq);

      commandSign = getApduCommand(Ledger::CLA, Ledger::INS_HASH_INPUT_START, 0x80, 0x80, std::move(inputPayload));
      inputSignCommands.push_back(std::move(commandSign));


      auto &path = inputPaths_[i];
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

      commandSign = getApduCommand(Ledger::CLA, Ledger::INS_HASH_SIGN, 0x00, 0x00, std::move(signPayload));
      inputSignCommands.push_back(std::move(commandSign));
   }

   QVector<QByteArray> responseSigned;
   for (auto &inputCommandSign : inputSignCommands) {
      QByteArray responseInputSign;
      if (!exchangeData(inputCommandSign, responseInputSign, "[LedgerCommandThread] signTX - Sign Payload")) {
         releaseDevice();
         emit error();
         return;
      }
      if (inputCommandSign[1] == static_cast<char>(Ledger::INS_HASH_SIGN)) {
         responseSigned.push_back(responseInputSign);
      }
   }

   /////////////
   // Composing and send data back
   Blocksettle::Communication::headless::InputSigs sigs;
   for (std::size_t i = 0; i < responseSigned.size(); ++i) {
      auto &signedInput = responseSigned[i];
      auto pubKey = inputNodes[i].getPublicKey();
      Asset_PublicKey pubKeyAsset(pubKey);
      auto compressedKey = pubKeyAsset.getCompressedKey();

      QByteArray composedData;
      composedData.append(static_cast<char>(signedInput.size()));
      composedData.append(signedInput);
      composedData.append(static_cast<char>(compressedKey.getSize()));
      composedData.push_back(QByteArray::fromStdString(compressedKey.toBinStr()));

      auto *sig = sigs.add_inputsig();
      sig->set_index(static_cast<uint>(i));
      sig->set_data(composedData.toStdString().c_str(), composedData.size());
   }

   HWSignedTx wrapper;
   wrapper.signedTx = sigs.SerializeAsString();
   emit resultReady(QVariant::fromValue(wrapper));
}

bool LedgerCommandThread::initDevice()
{
   if (hid_init() < 0) {
      logger_->info(
         "[LedgerCommandThread] getPublicKey - Cannot init hid.");
      emit error();
      return false;
   }

   std::unique_ptr<wchar_t> serNumb(new wchar_t[hidDeviceInfo_.serialNumber_.length() + 1]);
   hidDeviceInfo_.serialNumber_.toWCharArray(serNumb.get());
   serNumb.get()[hidDeviceInfo_.serialNumber_.length()] = 0x00;
   dongle_ = nullptr;
   dongle_ = hid_open(static_cast<ushort>(Ledger::HID_VENDOR_ID), static_cast<ushort>(hidDeviceInfo_.productId_), serNumb.get());

   return dongle_ != nullptr;
}

void LedgerCommandThread::releaseDevice()
{
   if (dongle_) {
      hid_close(dongle_);
      hid_exit();
      dongle_ = nullptr;
   }
}

bool LedgerCommandThread::exchangeData(const QByteArray& input,
   QByteArray& output, std::string&& logHeader)
{
   auto logHeaderCopy = logHeader;
   if (!writeData(input, std::move(logHeader))) {
      return false;
   }

   return readData(output, std::move(logHeaderCopy));
}

bool LedgerCommandThread::writeData(const QByteArray& input, std::string&& logHeader)
{
   logger_->debug(logHeader + " - >>> " + input.toHex().toStdString());
   if (sendApdu(dongle_, input) < 0) {
      logger_->debug(
         logHeader + " - Cannot write to device.");
      emit error();
      return false;
   }

   return true;
}

bool LedgerCommandThread::readData(QByteArray& output, std::string&& logHeader)
{
   
   auto res = receiveApduResult(dongle_, output);
   if (res != Ledger::SW_OK) {
      logger_->debug(
         logHeader + " - Cannot read from device. APDU error code : "
         + QByteArray::number(res, 16).toStdString());
      emit error();
      return false;
   }

   logger_->debug(logHeader + " - <<< " + BinaryData::fromString(output.toStdString()).toHexStr() + "9000");
   return true;
}
