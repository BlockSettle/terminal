/*

***********************************************************************************
* Copyright (C) 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef JADE_DEVICE_H
#define JADE_DEVICE_H

#include "Message/Worker.h"
#include "hwdeviceinterface.h"
#include "jadeClient.h"

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

namespace bs {
   namespace hww {

      class JadeDevice : public DeviceInterface, protected WorkerPool
      {
      public:
         JadeDevice(const std::shared_ptr<spdlog::logger>&
            , bool testNet, DeviceCallbacks*, const std::string& endpoint);
         ~JadeDevice() override;

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
         //std::shared_ptr<Worker> worker(const std::shared_ptr<InData>&) override final;

         // operation result informing
         void publicKeyReady() override {}   //TODO: implement
         void deviceTxStatusChanged(const std::string& status) override {} //TODO: implement
         void operationFailed(const std::string& reason) override;
         void requestForRescan() override {} //TODO: implement

         // Management
         void cancelledOnDevice() override {}   //TODO: implement
         void invalidPin() override {}    //TODO: implement

      private:
         void reset();

      private:
         std::shared_ptr<spdlog::logger>  logger_;
         const trezor::DeviceData data_;
         const bool        testNet_;
         DeviceCallbacks*  cb_{ nullptr };
         const std::string endpoint_;
         bs::core::HwWalletInfo  awaitingWalletInfo_;
         std::string awaitingSignedTX_;
      };

   }  //hw
}     //bs
#endif // JADE_DEVICE_H
