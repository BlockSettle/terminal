/*

***********************************************************************************
* Copyright (C) 2020 - 2021, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef LEDGERDEVICE_H
#define LEDGERDEVICE_H

#include <map>
#include "BinaryData.h"
#include "hwdeviceinterface.h"
#include "ledger/ledgerStructure.h"
#include "ledger/hidapi/hidapi.h"
#include "Message/Worker.h"

namespace spdlog {
   class logger;
}
namespace bs {
   namespace core {
      namespace wallet {
         struct TXSignRequest;
      }
   }

   namespace hww {
      struct DeviceCallbacks;

      class LedgerDevice : public DeviceInterface, protected WorkerPool
      {
      public:
         LedgerDevice(const HidDeviceInfo& hidDeviceInfo, bool testNet
            , const std::shared_ptr<spdlog::logger>&, DeviceCallbacks*
            , const std::shared_ptr<std::mutex>& hidLock);
         ~LedgerDevice() override = default;

         DeviceKey key() const override;
         DeviceType type() const override;

         // lifecycle
         void init() override;
         void clearSession() {}
         void cancel() override { WorkerPool::cancel(); }

         // operation
         void getPublicKeys() override;
         void signTX(const bs::core::wallet::TXSignRequest& reqTX) override;
         void retrieveXPubRoot() override;

         bool isBlocked() const override {
            return isBlocked_;
         }
         std::string lastError() const override {
            return lastError_;
         }

      protected:
         void requestForRescan() override;
         std::shared_ptr<Worker> worker(const std::shared_ptr<InData>&) override;

         // operation result informing
         void publicKeyReady() override {}   //TODO: implement
         void deviceTxStatusChanged(const std::string& status) override {} //TODO: implement
         void operationFailed(const std::string& reason) override {} //TODO: implement

         // Management
         void cancelledOnDevice() override {}   //TODO: implement
         void invalidPin() override {}    //TODO: implement

      private:
         void handleError(int32_t errorCode);

      private:
         const HidDeviceInfo  hidDeviceInfo_;
         const bool     testNet_;
         std::shared_ptr<spdlog::logger> logger_;
         DeviceCallbacks*  cb_{ nullptr };
         bool isBlocked_{false};
         std::string lastError_{};
         std::shared_ptr<std::mutex> hidLock_;
      };


      class DeviceIOHandler
      {
      public:
         DeviceIOHandler(const std::shared_ptr<spdlog::logger>& logger
            , const HidDeviceInfo& devInfo, const std::shared_ptr<std::mutex>& hidLock)
            : logger_(logger), hidDeviceInfo_(devInfo), hidLock_(hidLock)
         {}
         ~DeviceIOHandler() {
            releaseDevice();
         }

         bool initDevice() noexcept;
         void releaseDevice() noexcept;
         bool writeData(const QByteArray& input, const std::string& logHeader) noexcept;
         void readData(QByteArray& output, const std::string& logHeader);
         bool exchangeData(const QByteArray& input, QByteArray& output, std::string&& logHeader);

      protected:
         std::shared_ptr<spdlog::logger>  logger_;
         const HidDeviceInfo              hidDeviceInfo_;
         std::shared_ptr<std::mutex>      hidLock_;
         hid_device* dongle_{ nullptr };
      };

      struct SignTXIn : public bs::InData
      {
         ~SignTXIn() override = default;
         DeviceKey   key;
         bs::core::wallet::TXSignRequest  txReq;
         std::vector<bs::hd::Path>        inputPaths;
         bs::hd::Path                     changePath;
         std::vector<BIP32_Node>          inputNodes;
      };
      struct SignTXOut : public bs::OutData
      {
         ~SignTXOut() override = default;
         SecureBinaryData  serInputSigs;
      };

      class SignTXHandler : public bs::HandlerImpl<SignTXIn, SignTXOut>, protected DeviceIOHandler
      {
      public:
         SignTXHandler(const std::shared_ptr<spdlog::logger>& logger
            , const HidDeviceInfo& devInfo, const std::shared_ptr<std::mutex>& hidLock)
            : DeviceIOHandler(logger, devInfo, hidLock) {}

      protected:
         std::shared_ptr<SignTXOut> processData(const std::shared_ptr<SignTXIn>&) override;

      private:
         void processTXLegacy(const std::shared_ptr<SignTXIn>&, const std::shared_ptr<SignTXOut>&);
         void processTXSegwit(const std::shared_ptr<SignTXIn>&, const std::shared_ptr<SignTXOut>&);
         QByteArray getTrustedInput(const std::shared_ptr<SignTXIn>&, const BinaryData& hash
            , unsigned txOutId);
         SegwitInputData getSegwitData(const std::shared_ptr<SignTXIn>&);
         void sendTxSigningResult(const std::shared_ptr<SignTXOut>&, const std::map<int, QByteArray>&);
         void startUntrustedTransaction(const std::vector<QByteArray>& trustedInputs
            , const std::vector<QByteArray>& redeemScripts, unsigned txOutIndex
            , bool isNew, bool isSW, bool isRbf);
         void finalizeInputFull(const std::shared_ptr<SignTXIn>&);
         void debugPrintLegacyResult(const std::shared_ptr<SignTXIn>&
            , const QByteArray& responseSigned, const BIP32_Node&);
      };

      struct PubKeyIn : public bs::InData
      {
         ~PubKeyIn() override = default;
         std::vector<bs::hd::Path>  paths;
      };
      struct PubKeyOut : public bs::OutData
      {
         ~PubKeyOut() override = default;
         std::vector<BIP32_Node> pubKeys;
      };

      class GetPubKeyHandler : public bs::HandlerImpl<PubKeyIn, PubKeyOut>, protected DeviceIOHandler
      {
      public:
         GetPubKeyHandler(const std::shared_ptr<spdlog::logger>& logger
            , const HidDeviceInfo& devInfo, const std::shared_ptr<std::mutex>& hidLock)
            : DeviceIOHandler(logger, devInfo, hidLock)
         {}

      protected:
         BIP32_Node getPublicKeyApdu(const bs::hd::Path&, const std::unique_ptr<BIP32_Node>& parent);
         BIP32_Node retrievePublicKeyFromPath(const bs::hd::Path&);

         std::shared_ptr<PubKeyOut> processData(const std::shared_ptr<PubKeyIn>&) override;
      };

   }  //hw
}     //bs

#endif // LEDGERDEVICE_H
