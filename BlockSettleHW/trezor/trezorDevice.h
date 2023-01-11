/*

***********************************************************************************
* Copyright (C) 2020 - 2021, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef TREZORDEVICE_H
#define TREZORDEVICE_H

#include "Message/Worker.h"
#include "trezorStructure.h"
#include "hwdeviceinterface.h"
#include "trezorClient.h"

namespace spdlog {
   class logger;
}
namespace bs {
   namespace core {
      namespace wallet {
         struct TXSignRequest;
      }
   }
}
namespace hw {
   namespace trezor {
      namespace messages {
         namespace bitcoin {
            class TxRequest;
         }
         namespace management {
            class Features;
            enum Features_Capability: int;
         }
         enum MessageType: int;
      }
   }
}

namespace bs {
   namespace hww {

      class TrezorDevice : public DeviceInterface, protected WorkerPool
      {
      public:
         TrezorDevice(const std::shared_ptr<spdlog::logger>&, const trezor::DeviceData&
            , bool testNet, DeviceCallbacks*, const std::string& endpoint);
         ~TrezorDevice() override;

         trezor::DeviceData data() const { return data_; }
         DeviceKey key() const override;
         DeviceType type() const override;

         // lifecycle
         void init() override;
         void cancel() override;
         void clearSession() override;
         void releaseConnection();

         // operation
         void getPublicKeys() override;
         void signTX(const bs::core::wallet::TXSignRequest& reqTX) override;
         void retrieveXPubRoot() override;

         // Management
         void setMatrixPin(const SecureBinaryData& pin) override;
         void setPassword(const SecureBinaryData& password, bool enterOnDevice) override;

         // State
         bool isBlocked() const override {
            // There is no blocking state for Trezor
            return false;
         }

      protected:
         std::shared_ptr<Worker> worker(const std::shared_ptr<InData>&) override final;

         // operation result informing
         void publicKeyReady() override {}   //TODO: implement
         void deviceTxStatusChanged(const std::string& status) override {} //TODO: implement
         void operationFailed(const std::string& reason) override;
         void requestForRescan() override {} //TODO: implement

         // Management
         void requestPinMatrix() override {} //TODO: implement
         void requestHWPass(bool allowedOnDevice) override {} //TODO: implement
         void cancelledOnDevice() override {}   //TODO: implement
         void invalidPin() override {}    //TODO: implement

      private:
         void makeCall(const google::protobuf::Message&, const bs::WorkerPool::callback& cb = nullptr);
         void handleMessage(const trezor::MessageData&, const bs::WorkerPool::callback& cb = nullptr);
         bool parseResponse(google::protobuf::Message&, const trezor::MessageData&);

         void reset();

         void handleTxRequest(const trezor::MessageData&, const bs::WorkerPool::callback& cb);
         void sendTxMessage(const std::string& status);

         // Returns previous Tx for legacy inputs
         // Trezor could request non-existing hash if wrong passphrase entered
         Tx prevTx(const ::hw::trezor::messages::bitcoin::TxRequest& txRequest);

         bool hasCapability(const ::hw::trezor::messages::management::Features_Capability&) const;
         bool isFirmwareSupported() const;
         std::string firmwareSupportedVersion() const;

      private:
         std::shared_ptr<spdlog::logger>  logger_;
         const trezor::DeviceData data_;
         const bool        testNet_;
         DeviceCallbacks*  cb_{ nullptr };
         const std::string endpoint_;
         trezor::State     state_{ trezor::State::None };
         std::shared_ptr<::hw::trezor::messages::management::Features> features_{};

         std::unique_ptr<bs::core::wallet::TXSignRequest> currentTxSignReq_;
         bs::core::wallet::HwWalletInfo   awaitingWalletInfo_;
         std::string awaitingSignedTX_;
         bool txSignedByUser_{ false };
      };

   }  //hw
}     //bs
#endif // TREZORDEVICE_H
