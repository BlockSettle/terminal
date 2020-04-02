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

#include "QByteArray"
#include "QDataStream"
#include "QDebug"

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
      chunks.push_back(current);

      int nextByte = command.size() - (Ledger::FIRST_BLOCK_SIZE);
      for (; nextByte > 0; nextByte -= Ledger::NEXT_BLOCK_SIZE) {
         current.clear();
         writeUintBE(current, Ledger::CHANNEL);
         writeUintBE(current, Ledger::TAG_APDU);
         writeUintBE(current, chunkNumber++);

         current.append(command.mid(0, Ledger::NEXT_BLOCK_SIZE));
         chunks.push_back(current);
      }

      QByteArray padding;
      padding.fill(0, std::abs(nextByte));
      chunks.last().append(padding);

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

      response.append(chunk.mid(Ledger::FIRST_BLOCK_OFFSET));
      left -= Ledger::FIRST_BLOCK_SIZE;

      for (; left > 0; left -= Ledger::NEXT_BLOCK_SIZE) {
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

   std::vector<uint32_t> getDerivationPath(bool testNet, bool isNestedSegwit)
   {
      std::vector<uint32_t> path;
      if (isNestedSegwit) {
         path.push_back(0x80000031);
      }
      else {
         path.push_back(0x80000054);
      }

      if (testNet) {
         path.push_back(0x80000001);
      }
      else {
         path.push_back(0x80000000);
      }

      path.push_back(0x80000000);

      return path;
   }
}

LedgerDevice::LedgerDevice(HidDeviceInfo&& hidDeviceInfo, bool testNet, std::shared_ptr<spdlog::logger> logger, QObject* parent /*= nullptr*/)
   : HSMDeviceAbstract(parent)
   , hidDeviceInfo_(std::move(hidDeviceInfo))
   , logger_(logger)
   , testNet_(testNet)
{
}

DeviceKey LedgerDevice::key() const
{
   return {
      hidDeviceInfo_.productString_,
      hidDeviceInfo_.serialNumber_,
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
   if (hid_init() < 0) {
      logger_->info(
         "[LedgerClient] getPublicKey - Cannot init hid.");
      emit operationFailed();
      return;
   }

   std::unique_ptr<wchar_t> serNumb(new wchar_t[hidDeviceInfo_.serialNumber_.length() + 1]);
   hidDeviceInfo_.serialNumber_.toWCharArray(serNumb.get());
   serNumb.get()[hidDeviceInfo_.serialNumber_.length()] = 0x00;
   dongle_ = hid_open(static_cast<ushort>(Ledger::HID_VENDOR_ID), static_cast<ushort>(hidDeviceInfo_.productId_), serNumb.get());

   if (!dongle_) {
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

   auto pubKey = retrievePublicKey({ 0x80000000 });
   walletInfo.info_.xpubRoot_ = pubKey.getBase58().toBinStr();

   pubKey = retrievePublicKey(getDerivationPath(testNet_, true));
   walletInfo.info_.xpubNestedSegwit_ = pubKey.getBase58().toBinStr();

   pubKey = retrievePublicKey(getDerivationPath(testNet_, false));
   walletInfo.info_.xpubNativeSegwit_ = pubKey.getBase58().toBinStr();

   hid_close(dongle_);
   hid_exit();
   dongle_ = nullptr;

   if (cb) {
      cb(QVariant::fromValue<>(walletInfo));
   }
}

void LedgerDevice::signTX(const QVariant& reqTX, AsyncCallBackCall&& cb /*= nullptr*/)
{
}


BIP32_Node LedgerDevice::retrievePublicKey(std::vector<uint32_t>&& derivationPath)
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

   if (sendApdu(dongle_, command) < 0) {
      logger_->info(
         "[LedgerClient] getPublicKeyApdu - Cannot write to device.");
      emit operationFailed();
      return {};
   }

   QByteArray response;
   auto res = receiveApduResult(dongle_, response);
   qDebug() << response.toHex();
   if (res != Ledger::SW_OK) {
      logger_->info(
         "[LedgerClient] getPublicKeyApdu - Cannot read from device.");
      emit operationFailed();
      return {};
   }

   LedgerPublicKey pubKey;
   pubKey.parseFromResponse(response);

   Asset_PublicKey pubKeyAsset(SecureBinaryData::fromString(pubKey.pubKey_.toStdString()));
   SecureBinaryData chainCode = SecureBinaryData::fromString(pubKey.chainCode_.toStdString());

   uint32_t fingerprint = 0;
   if (parent) {
      auto pubkey_hash = BtcUtils::getHash160(parent->getPublicKey());
      uint32_t fingerprint = static_cast<uint32_t>(
         static_cast<uint32_t>(pubkey_hash[0] << 24) | static_cast<uint32_t>(pubkey_hash[1] << 16)
         | static_cast<uint32_t>(pubkey_hash[2] << 8) | static_cast<uint32_t>(pubkey_hash[3])
         );
   }

   BIP32_Node pubNode;
   pubNode.initFromPublicKey(derivationPath.size(), derivationPath.back(),
      fingerprint, pubKeyAsset.getCompressedKey(), chainCode);

   return pubNode;
}
