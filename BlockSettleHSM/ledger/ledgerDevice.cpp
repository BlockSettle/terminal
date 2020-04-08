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
      qDebug() << "Before send : " << command.toHex();
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
         qDebug() << chunk.toHex();
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
      qDebug() << buff;

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
   std::shared_ptr<bs::sync::WalletsManager> walletManager, std::shared_ptr<spdlog::logger> logger, QObject* parent /*= nullptr*/)
   : HSMDeviceAbstract(parent)
   , hidDeviceInfo_(std::move(hidDeviceInfo))
   , logger_(logger)
   , testNet_(testNet)
   , walletManager_(walletManager)
{
}

LedgerDevice::~LedgerDevice()
{
   releaseDevice();
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
   processGetPublicKey(std::move(cb));
   releaseDevice();
}

void LedgerDevice::signTX(const QVariant& reqTX, AsyncCallBackCall&& cb /*= nullptr*/)
{
   if (!initDevice()) {
      logger_->info(
         "[LedgerClient] getPublicKey - Cannot open device.");
      emit operationFailed();
      return;
   }

   Blocksettle::Communication::headless::SignTxRequest request;
   if (!request.ParseFromString(reqTX.toByteArray().toStdString())) {
      logger_->debug("[LedgerDevice] signTX - failed to parse transaction request ");
      return;
   }

   bs::core::wallet::TXSignRequest coreReq = bs::signer::pbTxRequestToCore(request);

   // Do not delete this comment!
   // Flow :
   // - we send init payload - INS_HASH_INPUT_START
   // - we send one by one all inputs - INS_HASH_INPUT_START
   // - we send one by one all outputs - INS_HASH_INPUT_FINALIZE
   // signing on device per output
   // - we send init payload again - INS_HASH_INPUT_START
   // - we send one by one all inputs - INS_HASH_INPUT_START and retrieve input sign for them

   // All this example is suppose to work on such case:
   // 1 Native Input to 1 Native Output in the same wallet without change

   // -- Start Init section --
   qDebug() << "[LedgerClient] -- Start Init section --";
   QByteArray initPayload(reinterpret_cast<const char*>(Ledger::DEFAULT_VERSION.data()), Ledger::DEFAULT_VERSION.size());
   writeVarInt(initPayload, coreReq.inputs.size());
   auto initCommand = getApduCommand(Ledger::CLA, Ledger::INS_HASH_INPUT_START, 0x00, 0x02, std::move(initPayload));

   QByteArray responseInit;
   if (!exchangeData(initCommand, responseInit, "[LedgerClient] signTX - initPayload")) {
      releaseDevice();
      return;
   }
   // -- End Init section --

   // -- Start input section --

   qDebug() << "[LedgerClient] -- Start input section --";
   QVector<QByteArray> inputCommands;
   uint32_t sequenceId = 0;
   QByteArray script;
   for (auto &utxo: coreReq.inputs) {
      QByteArray inputPayload;
      inputPayload.append(static_cast<char>(0x02));
      inputPayload.append(QByteArray::fromStdString(utxo.getTxHash().toBinStr()));
      writeUintLE(inputPayload, utxo.getTxOutIndex());
      writeUintLE(inputPayload, utxo.getValue());
      script = QByteArray::fromStdString(utxo.getScript().toBinStr());
      writeVarInt(inputPayload, static_cast<uint32_t>(script.size())); // script ????
      inputPayload.append(script);
      writeUintLE(inputPayload, std::numeric_limits<uint32_t>::max());

      auto command = getApduCommand(Ledger::CLA, Ledger::INS_HASH_INPUT_START, 0x80, 0x02, std::move(inputPayload));
      inputCommands.push_back(std::move(command));
   }

   for (auto &inputCommand : inputCommands) {
      QByteArray responseInput;
      if (!exchangeData(inputCommand, responseInput, "[LedgerClient] signTX - inputPayload")) {
         releaseDevice();
         return;
      }
   }

   // -- End input section --

   // -- Start output section --
   
   qDebug() << "[LedgerClient] -- Start output section --";

   // Need code here for change

   QByteArray outputFullPayload;
   writeVarInt(outputFullPayload, coreReq.recipients.size());
   for (auto &recipient : coreReq.recipients) {
      outputFullPayload.push_back(QByteArray::fromStdString(recipient->getSerializedScript().toBinStr()));
   }

   QVector<QByteArray> outputCommands;
   for (int proccessed = 0; proccessed < outputFullPayload.size(); proccessed += Ledger::OUT_CHUNK_SIZE) {
      uint8_t p1 = (proccessed + Ledger::OUT_CHUNK_SIZE > outputFullPayload.size()) ? 0x00 : 0x80;

      auto chunk = outputFullPayload.mid(proccessed, Ledger::OUT_CHUNK_SIZE);
      auto outputCommand = getApduCommand(Ledger::CLA, Ledger::INS_HASH_INPUT_FINALIZE_FULL,
         p1, 0x00, std::move(chunk));
      outputCommands.push_back(std::move(outputCommand));
   }

   for (auto &outputCommand : outputCommands) {
      QByteArray responseOutput;
      if (!exchangeData(outputCommand, responseOutput, "[LedgerClient] signTX - outputPayload")) {
         releaseDevice();
         return;
      }

      qDebug() << responseOutput;
   }

   // -- End output section --


   // In this point user verified all outputs and we could start signing inputs

   // -- Start Init section 2 --
   qDebug() << "[LedgerClient] -- Start Init section --";
   QByteArray initPayload2(reinterpret_cast<const char*>(Ledger::DEFAULT_VERSION.data()), Ledger::DEFAULT_VERSION.size());
   writeVarInt(initPayload2, coreReq.inputs.size());
   auto initCommand2 = getApduCommand(Ledger::CLA, Ledger::INS_HASH_INPUT_START, 0x00, 0x80, std::move(initPayload2));

   QByteArray responseInit2;
   if (!exchangeData(initCommand2, responseInit2, "[LedgerClient] signTX - initPayload")) {
      releaseDevice();
      return;
   }
   // -- End Init section --

   qDebug() << "[LedgerClient] -- Start INS_HASH_INPUT_START section--";

   QVector<QByteArray> inputCommands2;
   //uint32_t sequenceId2 = 0;
   for (auto &utxo : coreReq.inputs) {
      QByteArray inputPayload;
      inputPayload.append(static_cast<char>(0x02));
      inputPayload.append(QByteArray::fromStdString(utxo.getTxHash().toBinStr()));
      writeUintLE(inputPayload, utxo.getTxOutIndex());
      writeUintLE(inputPayload, utxo.getValue());
      script = QByteArray::fromStdString(utxo.getScript().toBinStr());
      writeVarInt(inputPayload, static_cast<uint32_t>(script.size())); // script ????
      inputPayload.append(script);
      writeUintLE(inputPayload, std::numeric_limits<uint32_t>::max());

      auto command = getApduCommand(Ledger::CLA, Ledger::INS_HASH_INPUT_START, 0x80, 0x02, std::move(inputPayload));
      inputCommands2.push_back(std::move(command));
   }

   for (auto &inputCommand : inputCommands2) {
      QByteArray responseInput;
      if (!exchangeData(inputCommand, responseInput, "[LedgerClient] signTX - inputPayload")) {
         releaseDevice();
         return;
      }
   }

   //Only for one transaction now
   qDebug() << "[LedgerClient] -- Start INS_HASH_SIGN section--";
   auto address = bs::Address::fromUTXO(coreReq.inputs[0]);
   bool isNestedSegwit = (address.getType() == AddressEntryType_P2SH);

   std::string addrIndex;
   bs::sync::WalletsManager::WalletPtr hswWallet = nullptr;
   for (const auto &walletId : coreReq.walletIds) {
      auto wallet = walletManager_->getWalletById(walletId);
      addrIndex = wallet->getAddressIndex(address);
      if (!addrIndex.empty()) {
         hswWallet = wallet;
         break;
      }
   }

   QByteArray derivationPath;
   std::vector<uint32_t> pathDer;
   for (const uint32_t add : getDerivationPath(testNet_, isNestedSegwit)) {
      writeUintBE(derivationPath, add);
      pathDer.push_back(add);
   }

   auto pubKeyAddress = retrievePublicKeyFromPath(getDerivationPath(testNet_, false));
   const auto path = bs::hd::Path::fromString(addrIndex);
   for (int i = 0; i < path.length(); ++i) {
      writeUintBE(derivationPath, path.get(i));
      pubKeyAddress.derivePublic(path.get(i));
      pathDer.push_back(path.get(i));
   }

   auto pubKeyAddress2 = retrievePublicKeyFromPath(std::move(pathDer));

   qDebug() << "Our :    " << QString::fromStdString(pubKeyAddress.getPublicKey().toHexStr());
   qDebug() << "Device : " << QString::fromStdString(pubKeyAddress2.getPublicKey().toHexStr());
   
   
   /////

   QByteArray inputSigPayload;
   inputSigPayload.append(static_cast<char>(0x05));
   inputSigPayload.append(derivationPath);
   inputSigPayload.append(static_cast<char>(0x00));
   writeUintBE(inputSigPayload, static_cast<uint32_t>(0x00));
   inputSigPayload.append(static_cast<char>(0x01));

   auto command = getApduCommand(Ledger::CLA, Ledger::INS_HASH_SIGN, 0x00, 0x00, std::move(inputSigPayload));
   QByteArray inputSigFinal;
   if (!exchangeData(command, inputSigFinal, "[LedgerClient] signTX - inputPayload")) {
      releaseDevice();
      return;
   }
   inputSigFinal[0] = 0x30;

   qDebug() << "inputSig " << QString::fromStdString(BinaryData::fromString(inputSigFinal.toStdString()).toHexStr());


   ///////////// 
   // Composing and send data back

   auto data = pubKeyAddress.getPublicKey();
   Asset_PublicKey pubKeyAsset(data);
   auto compressedKey = pubKeyAsset.getCompressedKey();

   qDebug() << "Second part size " << compressedKey.getSize() << " " << QString::fromStdString(compressedKey.toHexStr());

   QByteArray finalStructure;
   finalStructure.append(static_cast<char>(inputSigFinal.size()));
   finalStructure.append(inputSigFinal);
   finalStructure.append(static_cast<char>(compressedKey.getSize()));
   finalStructure.push_back(QByteArray::fromStdString(compressedKey.toBinStr()));

   qDebug() << "Final : " << finalStructure.toHex();

   //

   //
   if (cb) {
      HSMSignedTx signedTx{ finalStructure.toStdString() };
      cb(QVariant::fromValue(signedTx));
   }

   releaseDevice();
}

void LedgerDevice::processGetPublicKey(AsyncCallBackCall&& cb /*= nullptr*/)
{
   if (!initDevice()) {
      logger_->info(
         "[LedgerClient] getPublicKey - Cannot open device.");
      emit operationFailed();
      return;
   }

   auto deviceKey = key();
   HSMWalletWrapper walletInfo;
   walletInfo.info_.vendor_ = deviceKey.vendor_.toStdString();
   walletInfo.info_.label_ = deviceKey.deviceLabel_.toStdString();
   walletInfo.info_.deviceId_ = deviceKey.deviceId_.toStdString();

   auto pubKey = retrievePublicKeyFromPath({ 0x80000000 });
   try {
      walletInfo.info_.xpubRoot_ = pubKey.getBase58().toBinStr();
   }
   catch (...) {
      logger_->info(
         "[LedgerClient] getPublicKey - Cannot retrieve root xpub key.");
      emit operationFailed();
      return;
   }

   pubKey = retrievePublicKeyFromPath(getDerivationPath(testNet_, true));
   try {
      walletInfo.info_.xpubNestedSegwit_ = pubKey.getBase58().toBinStr();
   }
   catch (...) {
      logger_->info(
         "[LedgerClient] getPublicKey - Cannot retrieve nested segwit xpub key.");
      emit operationFailed();
      return;
   }

   pubKey = retrievePublicKeyFromPath(getDerivationPath(testNet_, false));
   try {
      walletInfo.info_.xpubNativeSegwit_ = pubKey.getBase58().toBinStr();
   }
   catch (...) {
      logger_->info(
         "[LedgerClient] getPublicKey - Cannot retrieve native segwit xpub key.");
      emit operationFailed();
      return;
   }

   if (!walletInfo.isValid()) {
      logger_->info(
         "[LedgerClient] getPublicKey - Wallet info is not correct.");
      emit operationFailed();
      return;
   }
   else {
      logger_->info(
         "[LedgerClient] getPublicKey - Operation succeeded.\nRoot xpub : "
         + walletInfo.info_.xpubRoot_ + " \nNested xpub: "
         + walletInfo.info_.xpubNestedSegwit_ + " \nNativeSegwit: "
         + walletInfo.info_.xpubNativeSegwit_);
   }

   if (cb) {
      cb(QVariant::fromValue<>(walletInfo));
   }
}

BIP32_Node LedgerDevice::retrievePublicKeyFromPath(std::vector<uint32_t>&& derivationPath)
{
   // Parent
   std::unique_ptr<BIP32_Node> parent = nullptr;
   if (derivationPath.size() > 1) {
      std::vector<uint32_t> parentPath(derivationPath.begin(), derivationPath.end() - 1);
      parent.reset(new BIP32_Node(getPublicKeyApdu(std::move(parentPath))));
   }

   return getPublicKeyApdu(std::move(derivationPath), parent);
}

BIP32_Node LedgerDevice::getPublicKeyApdu(std::vector<uint32_t>&& derivationPath, const std::unique_ptr<BIP32_Node>& parent)
{
   QByteArray payload;
   payload.append(derivationPath.size());
   for (auto key : derivationPath) {
      writeUintBE(payload, key);
   }

   QByteArray command = getApduHeader(Ledger::CLA, Ledger::INS_GET_WALLET_PUBLIC_KEY, 0, 0);
   command.append(static_cast<char>(payload.size()));
   command.append(payload);

   QByteArray response;
   if (!exchangeData(command, response, "[LedgerClient] getPublicKeyApdu - ")) {
      emit operationFailed();
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
   pubNode.initFromPublicKey(derivationPath.size(), derivationPath.back(),
      fingerprint, pubKeyAsset.getCompressedKey(), chainCode);

   return pubNode;
}

bool LedgerDevice::initDevice()
{
   if (hid_init() < 0) {
      logger_->info(
         "[LedgerClient] getPublicKey - Cannot init hid.");
      emit operationFailed();
      return false;
   }

   std::unique_ptr<wchar_t> serNumb(new wchar_t[hidDeviceInfo_.serialNumber_.length() + 1]);
   hidDeviceInfo_.serialNumber_.toWCharArray(serNumb.get());
   serNumb.get()[hidDeviceInfo_.serialNumber_.length()] = 0x00;
   dongle_ = nullptr;
   dongle_ = hid_open(static_cast<ushort>(Ledger::HID_VENDOR_ID), static_cast<ushort>(hidDeviceInfo_.productId_), serNumb.get());

   return dongle_ != nullptr;
}

void LedgerDevice::releaseDevice()
{
   if (dongle_) {
      hid_close(dongle_);
      hid_exit();
      dongle_ = nullptr;
   }
}

bool LedgerDevice::exchangeData(const QByteArray& input,
   QByteArray& output, std::string&& logHeader)
{
   auto logHeaderCopy = logHeader;
   if (!writeData(input, std::move(logHeader))) {
      return false;
   }

   return readData(output, std::move(logHeaderCopy));
}

bool LedgerDevice::writeData(const QByteArray& input, std::string&& logHeader)
{
   if (sendApdu(dongle_, input) < 0) {
      logger_->info(
         logHeader + " - Cannot write to device.");
      emit operationFailed();
      return false;
   }

   return true;
}

bool LedgerDevice::readData(QByteArray& output, std::string&& logHeader)
{
   auto res = receiveApduResult(dongle_, output);
   if (res != Ledger::SW_OK) {
      logger_->info(
         logHeader + " - Cannot read from device. APDU error code : "
         + QByteArray::number(res, 16).toStdString());
      emit operationFailed();
      return false;
   }

   return true;
}
