/*

***********************************************************************************
* Copyright (C) 2020 - 2021, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef HWDEVICESCANNER_H
#define HWDEVICESCANNER_H

#include <memory>
#include "HDPath.h"
#include "hwdeviceinterface.h"
#include "Message/Adapter.h"
#include "ledger/ledgerClient.h"
#include "trezor/trezorClient.h"
#include "jade/jadeClient.h"
#include "SecureBinaryData.h"


namespace spdlog {
   class logger;
}
namespace BlockSettle {
   namespace Common {
      class ArmoryMessage_Transactions;
      class HDWalletData;
      class SignerMessage_SignTxResponse;
   }
   namespace HW {
      class DeviceKey;
      class DeviceMgrMessage_SetPassword;
      class DeviceMgrMessage_SetPIN;
   }
}
namespace Blocksettle {
   namespace Communication {
      namespace headless {
         class SignTxRequest;
      }
   }
}

namespace bs {
   namespace hww {

      // There is no way to determinate difference between ledger devices
      // so we use vendor name for identification
      const std::string kDeviceLedgerId = "Ledger";

      struct DeviceCallbacks
      {
         virtual void publicKeyReady(const DeviceKey&) = 0;
         virtual void walletInfoReady(const DeviceKey&, const bs::core::HwWalletInfo&) = 0;
         virtual void requestPinMatrix(const DeviceKey&) = 0;
         virtual void requestHWPass(const DeviceKey&, bool allowedOnDevice) = 0;

         virtual void deviceNotFound(const std::string& deviceId) = 0;
         virtual void deviceReady(const std::string& deviceId) = 0;
         virtual void deviceTxStatusChanged(const std::string& status) = 0;

         virtual void needSupportingTXs(const DeviceKey&, const std::vector<BinaryData>& txHashes) = 0;

         virtual void txSigned(const DeviceKey&, const SecureBinaryData& signData) = 0;
         virtual void scanningDone() = 0;
         virtual void operationFailed(const std::string& deviceId, const std::string& reason) = 0;
         virtual void cancelledOnDevice() = 0;
         virtual void invalidPin() = 0;
      };

      class DeviceManager : public bs::message::Adapter, public DeviceCallbacks
      {
      public:
         DeviceManager(const std::shared_ptr<spdlog::logger>&);
         ~DeviceManager() override;

         bs::message::ProcessingResult process(const bs::message::Envelope&) override;
         bool processBroadcast(const bs::message::Envelope&) override;

         Users supportedReceivers() const override { return { user_ }; }
         std::string name() const override { return "HWWallets"; }

      private: // signals
         void publicKeyReady(const DeviceKey&) override;
         void walletInfoReady(const DeviceKey&, const bs::core::HwWalletInfo&) override;
         void requestPinMatrix(const DeviceKey&) override;
         void requestHWPass(const DeviceKey&, bool allowedOnDevice) override;

         void deviceNotFound(const std::string& deviceId) override;
         void deviceReady(const std::string& deviceId) override;
         void deviceTxStatusChanged(const std::string& status) override;
         void needSupportingTXs(const DeviceKey&, const std::vector<BinaryData>& txHashes) override;
         void txSigned(const DeviceKey&, const SecureBinaryData& signData) override;
         void scanningDone() override;
         void operationFailed(const std::string& deviceId, const std::string& reason) override;
         void cancelledOnDevice() override;
         void invalidPin() override;

         //former invokables:
         void scanDevices(const bs::message::Envelope&);
         void setMatrixPin(const DeviceKey&, const std::string& pin);
         void setPassphrase(const DeviceKey&, const std::string& passphrase
            , bool enterOnDevice);
         void cancel(const DeviceKey&);
         bs::message::ProcessingResult prepareDeviceForSign(bs::message::SeqId
            , const BlockSettle::Common::HDWalletData&);
         void signTX(const DeviceKey&, const bs::core::wallet::TXSignRequest& reqTX);
         void releaseDevices();
         //void hwOperationDone();
         bool awaitingUserAction(const DeviceKey&);

         void releaseConnection();
         void scanningDone(bool initDevices = true);

         std::shared_ptr<DeviceInterface> getDevice(const DeviceKey& key) const;

         void start();
         bs::message::ProcessingResult processPrepareDeviceForSign(const bs::message::Envelope&
            , const std::string& walletId);
         bs::message::ProcessingResult processOwnRequest(const bs::message::Envelope&);
         bs::message::ProcessingResult processWallet(const bs::message::Envelope&);
         bs::message::ProcessingResult processSettings(const bs::message::Envelope&);
         bs::message::ProcessingResult processSigner(const bs::message::Envelope&);
         bs::message::ProcessingResult processBlockchain(const bs::message::Envelope&);

         bs::message::ProcessingResult processTransactions(const bs::message::SeqId
            , const BlockSettle::Common::ArmoryMessage_Transactions&);

         void devicesResponse();
         bs::message::ProcessingResult processImport(const bs::message::Envelope&
            , const BlockSettle::HW::DeviceKey&);
         bs::message::ProcessingResult processSignTX(const bs::message::Envelope&
            , const Blocksettle::Communication::headless::SignTxRequest&);
         void signTxWithDevice(const DeviceKey&);
         bs::message::ProcessingResult processSignTxResponse(const BlockSettle::Common::SignerMessage_SignTxResponse&);
         bs::message::ProcessingResult processSetPIN(const BlockSettle::HW::DeviceMgrMessage_SetPIN&);
         bs::message::ProcessingResult processSetPassword(const BlockSettle::HW::DeviceMgrMessage_SetPassword&);

      private:
         std::shared_ptr<spdlog::logger> logger_;
         std::unique_ptr<TrezorClient> trezorClient_;
         std::unique_ptr<LedgerClient> ledgerClient_;
         std::unique_ptr<JadeClient>   jadeClient_;
         std::shared_ptr<bs::message::User>  user_, userWallets_, userSigner_, userBlockchain_;
         std::vector<DeviceKey>  devices_;
         mutable std::mutex      devMtx_;

         bool testNet_{false};
         int  nbScanning_{0};
         bool isSigning_{};
         std::string lastOperationError_;
         std::string lastUsedTrezorWallet_;
         unsigned nbWaitScanReplies_{ 0 };
         bs::message::Envelope   envReqScan_, envReqSign_;
         bs::core::wallet::TXSignRequest txSignReq_;

         std::map<bs::message::SeqId, std::pair<std::string, bs::message::Envelope>>   prepareDeviceReq_;   //value: walletId
         std::map<bs::message::SeqId, DeviceKey>   supportingTxReq_;
      };

      void deviceKeyToMsg(const DeviceKey&, BlockSettle::HW::DeviceKey*);
      DeviceKey fromMsg(const BlockSettle::HW::DeviceKey&);

      bs::hd::Path getDerivationPath(bool testNet, bs::hd::Purpose element);
      bool isNestedSegwit(const bs::hd::Path& path);
      bool isNativeSegwit(const bs::hd::Path& path);
      bool isNonSegwit(const bs::hd::Path& path);

   }  //hw
}  //bs

#endif // HWDEVICESCANNER_H
