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

#include <QCborMap>
#include <QObject>
#include <QtSerialPort>
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

      struct JadeSerialIn : public bs::InData
      {
         ~JadeSerialIn() override = default;
         QCborMap request;
      };
      struct JadeSerialOut : public bs::OutData
      {
         ~JadeSerialOut() override = default;
         std::future<QCborMap> futResponse;
      };

      class JadeSerialHandler : public QObject
         , public bs::HandlerImpl<JadeSerialIn, JadeSerialOut>
      {
         Q_OBJECT
      public:
         JadeSerialHandler(const std::shared_ptr<spdlog::logger>&
            , const QSerialPortInfo& spi);
         ~JadeSerialHandler() override;

      protected:
         std::shared_ptr<JadeSerialOut> processData(const std::shared_ptr<JadeSerialIn>&) override;

      private slots:
         void onSerialDataReady();  // Invoked when new serial data arrived

      private:
         bool Connect();
         void Disconnect();
         int write(const QByteArray&);
         void parsePortion(const QByteArray&);

      private:
         std::shared_ptr<spdlog::logger>  logger_;
         QSerialPort* serial_{ nullptr };
         std::deque<std::shared_ptr<std::promise<QCborMap>>>   requests_;
         QByteArray unparsed_;
      };

      class JadeDevice : public DeviceInterface, protected WorkerPool
      {
      public:
         JadeDevice(const std::shared_ptr<spdlog::logger>&
            , bool testNet, DeviceCallbacks*, const QSerialPortInfo&);
         ~JadeDevice() override;

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
         void cancelledOnDevice() override {}   //TODO: implement
         void invalidPin() override {}    //TODO: implement

      private:
         void reset();

      private:
         std::shared_ptr<spdlog::logger>  logger_;
         const bool        testNet_;
         DeviceCallbacks*  cb_{ nullptr };
         const QSerialPortInfo   endpoint_;
         int   seqId_{ 0 };
         std::vector<std::shared_ptr<bs::Handler>> handlers_;
         bs::core::HwWalletInfo  awaitingWalletInfo_;
         std::string awaitingSignedTX_;
      };

   }  //hw
}     //bs
#endif // JADE_DEVICE_H
