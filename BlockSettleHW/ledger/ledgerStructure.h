/*

***********************************************************************************
* Copyright (C) 2020 - 2021, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef LEDGERSTRUCTURE_H
#define LEDGERSTRUCTURE_H

#include <unordered_set>
#include <QDataStream>
#include "BinaryData.h"
#include "BIP32_Node.h"


namespace Ledger {
   // HIDAPI data
   const uint16_t HID_VENDOR_ID_LEDGER_NANO_S = 0x2c97;
   const uint16_t HID_VENDOR_ID_LEDGER_NANO_X = 0x2581;
   const uint8_t  HID_INTERFACE_NUMBER = 0;
   const uint16_t HID_USAGE_PAGE = 0xFFA0;
   // https://github.com/LedgerHQ/btchip-python/blob/master/btchip/btchipComm.py#L213
   const std::unordered_set<uint16_t> HID_PRODUCT_ID_LEDGER_NANO_X {
      0x2b7c,
      0x3b7c,
      0x4b7c,
      0x1807
   };

   // APDU data
   const uint8_t CLA = 0xE0;
   const uint8_t ADM_CLA = 0xD0;

   const uint8_t INS_SETUP = 0x20;
   const uint8_t INS_VERIFY_PIN = 0x22;
   const uint8_t INS_GET_OPERATION_MODE = 0x24;
   const uint8_t INS_SET_OPERATION_MODE = 0x26;
   const uint8_t INS_SET_KEYMAP = 0x28;
   const uint8_t INS_SET_COMM_PROTOCOL = 0x2A;
   const uint8_t INS_GET_WALLET_PUBLIC_KEY = 0x40;
   const uint8_t INS_GET_TRUSTED_INPUT = 0x42;
   const uint8_t INS_HASH_INPUT_START = 0x44;
   const uint8_t INS_HASH_INPUT_FINALIZE = 0x46;
   const uint8_t INS_HASH_SIGN = 0x48;
   const uint8_t INS_HASH_INPUT_FINALIZE_FULL = 0x4A;
   const uint8_t INS_GET_INTERNAL_CHAIN_INDEX = 0x4C;
   const uint8_t INS_SIGN_MESSAGE = 0x4E;
   const uint8_t INS_GET_TRANSACTION_LIMIT = 0xA0;
   const uint8_t INS_SET_TRANSACTION_LIMIT = 0xA2;
   const uint8_t INS_IMPORT_PRIVATE_KEY = 0xB0;
   const uint8_t INS_GET_PUBLIC_KEY = 0xB2;
   const uint8_t INS_DERIVE_BIP32_KEY = 0xB4;
   const uint8_t INS_SIGNVERIFY_IMMEDIATE = 0xB6;
   const uint8_t INS_GET_RANDOM = 0xC0;
   const uint8_t INS_GET_ATTESTATION = 0xC2;
   const uint8_t INS_GET_FIRMWARE_VERSION = 0xC4;
   const uint8_t INS_COMPOSE_MOFN_ADDRESS = 0xC6;
   const uint8_t INS_DONGLE_AUTHENTICATE = 0xC8;
   const uint8_t INS_GET_POS_SEED = 0xCA;
   const uint8_t INS_ADM_INIT_KEYS = 0x20;
   const uint8_t INS_ADM_INIT_ATTESTATION = 0x22;
   const uint8_t INS_ADM_GET_UPDATE_ID = 0x24;
   const uint8_t INS_ADM_FIRMWARE_UPDATE = 0x42;

   // APDU return data
   const qint32 SW_OK = 0x9000;
   const qint32 SW_UNKNOWN = 0x6D00;
   const qint32 SW_NO_ENVIRONMENT = 0x6982;
   const qint32 SW_CANCELED_BY_USER = 0x6985;
   const qint32 SW_RECONNECT_DEVICE = 0x6FAA;
   const qint32 NO_DEVICE = -1;
   const qint32 NO_INPUTDATA = -2;
   const qint32 INTERNAL_ERROR = -3;

   // General
   const size_t OFFSET_CDATA = 4;
   const size_t FIRST_BLOCK_SIZE = 57;
   const size_t FIRST_BLOCK_OFFSET = 7;
   const size_t NEXT_BLOCK_SIZE = 59;
   const size_t NEXT_BLOCK_OFFSET = 5;

   const size_t CHUNK_MAX_BLOCK = 64;
   const size_t CHAIN_CODE_SIZE = 32;

   // Protocol
   const uint16_t CHANNEL = 0x0101;
   const uint8_t  TAG_APDU = 0x05;
   const uint8_t  TAG_PING = 0x02;

   // Tx
   const uint8_t PREVOUT_SIZE = 36;
   const uint32_t DEFAULT_VERSION = 1;
   const uint32_t DEFAULT_SEQUENCE = 0xffffffff;
   const uint8_t OUT_CHUNK_SIZE = 255;
   const uint8_t SEGWIT_TYPE = 0x02;
}

template <typename T,
   typename = typename std::enable_if<std::is_arithmetic<T>::value, T>::type>
void writeUintBE(QByteArray& out, T value) {
   if (sizeof(T) >= 8) {
      out.append(static_cast<char>((value >> 56) & 0xff));
      out.append(static_cast<char>((value >> 48) & 0xff));
      out.append(static_cast<char>((value >> 40) & 0xff));
      out.append(static_cast<char>((value >> 32) & 0xff));
   }

   if (sizeof(T) >= 4) {
      out.append(static_cast<char>((value >> 24) & 0xff));
      out.append(static_cast<char>((value >> 16) & 0xff));
   }

   if (sizeof(T) >= 2) {
      out.append(static_cast<char>((value >> 8) & 0xff));
   }

   out.append(static_cast<char>((value) & 0xff));
}

template <typename T,
   typename = typename std::enable_if<std::is_arithmetic<T>::value, T>::type>
void writeUintLE(QByteArray& out, T value) {
   out.append(static_cast<char>((value) & 0xff));

   if (sizeof(T) >= 2) {
      out.append(static_cast<char>((value >> 8) & 0xff));
   }

   if (sizeof(T) >= 4) {
      out.append(static_cast<char>((value >> 16) & 0xff));
      out.append(static_cast<char>((value >> 24) & 0xff));
   }

   if (sizeof(T) >= 8) {
      out.append(static_cast<char>((value >> 32) & 0xff));
      out.append(static_cast<char>((value >> 40) & 0xff));
      out.append(static_cast<char>((value >> 48) & 0xff));
      out.append(static_cast<char>((value >> 56) & 0xff));
   }
}

void writeVarInt(QByteArray &output, size_t size);

struct HidDeviceInfo {
   std::string path;
   uint16_t vendorId;
   uint16_t productId;
   std::string serialNumber;
   uint16_t releaseNumber;
   std::string manufacturer;
   std::string product;
   uint16_t usagePage;
   uint16_t usage;
   int      interfaceNumber;
};

struct SegwitInputData
{
   std::map<int, BinaryData>  preimages;
   std::map<int, BinaryData>  redeemScripts;

   bool empty() const
   {
      return (preimages.empty() && redeemScripts.empty());
   }
};

struct LedgerPublicKey
{
   QByteArray pubKey;
   QByteArray address;
   QByteArray chainCode;

   bool parseFromResponse(QByteArray response) {
      QDataStream stream(response);

      uint8_t pubKeyLength;
      stream >> pubKeyLength;

      pubKey.clear();
      pubKey.resize(pubKeyLength);
      stream.readRawData(pubKey.data(), pubKeyLength);

      uint8_t addressLength;
      stream >> addressLength;

      address.clear();
      address.resize(addressLength);
      stream.readRawData(address.data(), addressLength);

      chainCode.clear();
      chainCode.resize(Ledger::CHAIN_CODE_SIZE);
      stream.readRawData(chainCode.data(), Ledger::CHAIN_CODE_SIZE);

      return stream.atEnd();
   }

   bool isValid() {
      return !pubKey.isEmpty() && !address.isEmpty() && !chainCode.isEmpty();
   }
};

namespace HWInfoStatus {
   const QString kErrorNoDevice = QObject::tr("No device found");
   const QString kErrorInternalError = QObject::tr("Internal error");
   const QString kErrorNoEnvironment = QObject::tr("Please make sure you device is ready for using");
   const QString kErrorReconnectDevice= QObject::tr("Internal device error, please reconnect device to system");
}

struct hid_device_info;
bool checkLedgerDevice(hid_device_info* info);

#endif // LEDGERSTRUCTURE_H
