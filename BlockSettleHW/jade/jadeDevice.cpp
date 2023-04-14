/*

***********************************************************************************
* Copyright (C) 2023, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include <QCborArray>
#include <QCborMap>
#include <QCborValue>
#include <QCryptographicHash>
#include <QDataStream>
#include <spdlog/spdlog.h>
#include "hwdevicemanager.h"
#include "jadeDevice.h"
#include "jadeClient.h"
#include "CoreWallet.h"


using namespace bs::hww;

JadeDevice::JadeDevice(const std::shared_ptr<spdlog::logger> &logger
   , bool testNet, DeviceCallbacks* cb, const QSerialPortInfo& endpoint)
   : bs::WorkerPool(1, 1)
   , logger_(logger), testNet_(testNet), cb_(cb), endpoint_(endpoint)
{
   QMetaObject::invokeMethod(qApp, [this] {
      handlers_.push_back(std::make_shared<JadeSerialHandler>(logger_, endpoint_));
      });
}

JadeDevice::~JadeDevice() = default;

std::shared_ptr<bs::Worker> JadeDevice::worker(const std::shared_ptr<InData>&)
{
   return std::make_shared<bs::WorkerImpl>(handlers_);
}

void bs::hww::JadeDevice::operationFailed(const std::string& reason)
{
   releaseConnection();
   //cb_->operationFailed(features_->device_id(), reason);
}

void JadeDevice::releaseConnection()
{
}

DeviceKey JadeDevice::key() const
{
   std::string walletId;
   std::string status;
   if (!xpubRoot_.empty()) {
      walletId = bs::core::wallet::computeID(xpubRoot_).toBinStr();
   }
   else {
      status = "not inited";
   }
/*   return {features_->label(), features_->device_id(), features_->vendor()
      , walletId, status, type() };*/
   return {};
}

DeviceType JadeDevice::type() const
{
   return DeviceType::HWJade;
}

void JadeDevice::init()
{
   logger_->debug("[JadeDevice::init] start");
}

void JadeDevice::getPublicKeys()
{
   awaitingWalletInfo_ = {};
   // General data
   awaitingWalletInfo_.type = bs::wallet::HardwareEncKey::WalletType::Trezor;
/*   awaitingWalletInfo_.label = features_->label();
   awaitingWalletInfo_.deviceId = features_->device_id();
   awaitingWalletInfo_.vendor = features_->vendor();
   awaitingWalletInfo_.xpubRoot = xpubRoot_.toBinStr();*/

}

void JadeDevice::setMatrixPin(const SecureBinaryData& pin)
{
   logger_->debug("[JadeDevice::setMatrixPin]");
}

void JadeDevice::setPassword(const SecureBinaryData& password, bool enterOnDevice)
{
   logger_->debug("[JadeDevice::setPassword]");
}

void JadeDevice::cancel()
{
   logger_->debug("[JadeDevice] cancel previous operation");
}

void JadeDevice::clearSession()
{
   logger_->debug("[JadeDevice] cancel session");
}


void JadeDevice::signTX(const bs::core::wallet::TXSignRequest &reqTX)
{
   logger_->debug("[JadeDevice::signTX]");
}

void JadeDevice::retrieveXPubRoot()
{
   logger_->debug("[JadeDevice::retrieveXPubRoot]");
}

void JadeDevice::reset()
{
   awaitingSignedTX_.clear();
   awaitingWalletInfo_ = {};
}


// Helpers to build basic jade cbor request object
static inline QCborMap getRequest(const int id, const QString& method)
{
   QCborMap req;
   req.insert(QCborValue(QLatin1Literal("id")), QString::number(id));
   req.insert(QCborValue(QLatin1Literal("method")), method);
   return req;
}

static inline QCborMap getRequest(const int id, const QString& method
   , const QCborValue& params)
{
   QCborMap req(getRequest(id, method));
   req.insert(QCborValue(QLatin1Literal("params")), params);
   return req;
}


JadeSerialHandler::JadeSerialHandler(const std::shared_ptr<spdlog::logger>& logger
   , const QSerialPortInfo& spi)
   : QObject(nullptr), logger_(logger), serial_(new QSerialPort(spi, this))
{
   serial_->setBaudRate(QSerialPort::Baud115200);
   serial_->setDataBits(QSerialPort::Data8);
   serial_->setParity(QSerialPort::NoParity);
   serial_->setStopBits(QSerialPort::OneStop);

   if (!Connect()) {
      throw std::runtime_error("failed to open port " + spi.portName().toStdString());
   }
}

JadeSerialHandler::~JadeSerialHandler()
{
   Disconnect();
}

std::shared_ptr<JadeSerialOut> JadeSerialHandler::processData(const std::shared_ptr<JadeSerialIn>& in)
{
   auto out = std::make_shared<JadeSerialOut>();
   auto prom = std::make_shared<std::promise<QCborMap>>();
   out->futResponse = prom->get_future();
   requests_.push_back(prom);
   write(in->data);
   return out;
}

bool bs::hww::JadeSerialHandler::Connect()
{
   if (serial_->isOpen()) {
      return true;   // already connected
   }
   if (serial_->open(QIODevice::ReadWrite)) {
      // Connect 'data received' slot
      connect(serial_, &QSerialPort::readyRead, this
         , &JadeSerialHandler::onSerialDataReady);
      return true;
   }
   Disconnect();
   return false;
}

void bs::hww::JadeSerialHandler::Disconnect()
{
   disconnect(serial_, nullptr, this, nullptr);
   if (serial_->isOpen()) {
      serial_->close();
   }
}

int bs::hww::JadeSerialHandler::write(const QByteArray& data)
{
   assert(serial_);
   assert(serial_->isOpen());

   int written = 0;
   while (written != data.length()) {
      const auto wrote = serial_->write(data.data() + written, qMin(256, data.length() - written));
      if (wrote < 0) {
         Disconnect();
         return written;
      }
      else {
         serial_->waitForBytesWritten(100);
         written += wrote;
      }
      //std::this_thread::sleep_for(std::chrono::milliseconds{ 100 });
   }
   return written;
}

void bs::hww::JadeSerialHandler::parsePortion(const QByteArray& data)
{
   try {
      // Collect data
      unparsed_.append(data);

      // Try to parse cbor objects from byte buffer until it has no more complete objects
      for (bool readNextObj = true; readNextObj; /*nothing - set in loop*/) {
         QCborParserError err;
         const QCborValue cbor = QCborValue::fromCbor(unparsed_, &err);
         // qDebug() << "Read Type:" << cbor.type() << "and error: " << err.error;
         readNextObj = false;  // In most cases we don't read another object

         if (err.error == QCborError::NoError && cbor.isMap()) {
            const QCborMap msg = cbor.toMap();
            if (msg.contains(QCborValue(QLatin1Literal("log")))) {
               // Print Jade log line immediately
               logger_->info("[{}] {}", __func__, msg[QLatin1Literal("log")]
                  .toByteArray().toStdString());
            }
            else {
               requests_.front()->set_value(msg);
               requests_.pop_front();
            }

            // Remove read object from m_data buffer
            if (err.offset == unparsed_.length()) {
               unparsed_.clear();
            }
            else {
               // We successfully read an object and there are still bytes left in the buffer - this
               // is the one case where we loop and read again - make sure to preserve the remaining bytes.
               unparsed_ = unparsed_.right(static_cast<int>(unparsed_.length() - err.offset));
               readNextObj = true;
            }
         }
         else if (err.error == QCborError::EndOfFile) {
            // partial object - stop trying to read objects for now, await more data
            if (unparsed_.length() > 0) {
               logger_->debug("[{}] CBOR incomplete ({} bytes present) - awaiting more data"
                  , __func__, unparsed_.size());
            }
         }
         else {
            // Unexpected parse error
            logger_->warn("[{}] unexpected type {} and/or error: {}", __func__
               , (int)cbor.type(), err.error.toString().toStdString());
            Disconnect();
         }
      }
   }
   catch (const std::exception& e) {
      logger_->error("[{}] exception: {}", __func__, e.what());
      Disconnect();
   }
   catch (...) {
      logger_->error("[{}] exception", __func__);
      Disconnect();
   }
}

void JadeSerialHandler::onSerialDataReady()
{
   assert(serial_);
   const auto& data = serial_->readAll();

   if (requests_.empty()) {
      logger_->error("[{}] dropped {} bytes of serial data", __func__, data.size());
      return;
   }
   parsePortion(data);
}
