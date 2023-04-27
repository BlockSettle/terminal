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
struct curl_slist;

namespace bs {
   namespace hww {

      struct JadeSerialIn : public bs::InData
      {
         ~JadeSerialIn() override = default;
         QCborMap request;
         bool needResponse{ true };
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

      struct JadeHttpIn : public bs::InData
      {
         ~JadeHttpIn() override = default;
         std::string url;
         std::string data;
      };
      struct JadeHttpOut : public bs::OutData
      {
         ~JadeHttpOut() override = default;
         std::string response;
      };

      class JadeHttpHandler : public bs::HandlerImpl<JadeHttpIn, JadeHttpOut>
      {
      public:
         JadeHttpHandler(const std::shared_ptr<spdlog::logger>&);
         ~JadeHttpHandler() override;

      protected:
         std::shared_ptr<JadeHttpOut> processData(const std::shared_ptr<JadeHttpIn>&) override;

      private:
         std::shared_ptr<spdlog::logger>  logger_;
         struct curl_slist* curlHeaders_{ NULL };
         void* curl_{ nullptr };
      };


      class JadeDevice : public DeviceInterface, protected WorkerPool
      {
      public:
         JadeDevice(const std::shared_ptr<spdlog::logger>&
            , bool testNet, DeviceCallbacks*, const QSerialPortInfo&);
         ~JadeDevice() override;

         static std::string idFromSerial(const QSerialPortInfo&);
         DeviceKey key() const override;
         DeviceType type() const override;

         // lifecycle
         void init() override;
         void cancel() override { WorkerPool::cancel(); }
         void clearSession() override {}
         void releaseConnection();

         // operation
         void getPublicKeys() override;
         void signTX(const bs::core::wallet::TXSignRequest& reqTX) override;
         void retrieveXPubRoot() override;

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
         void setSupportingTXs(const std::vector<Tx>&) override;

         // Management
         void cancelledOnDevice() override {}   //TODO: implement
         void invalidPin() override {}    //TODO: implement

      private:
         void reset();
         QString network() const;

      private:
         std::shared_ptr<spdlog::logger>  logger_;
         const bool        testNet_;
         DeviceCallbacks*  cb_{ nullptr };
         const QSerialPortInfo   endpoint_;
         int   seqId_{ 0 };
         mutable std::string  walletId_;
         const std::vector<std::shared_ptr<bs::Handler>> handlers_;
         bs::core::HwWalletInfo           awaitingWalletInfo_;
         bs::core::wallet::TXSignRequest  awaitingTXreq_;
      };

   }  //hw
}     //bs
#endif // JADE_DEVICE_H
