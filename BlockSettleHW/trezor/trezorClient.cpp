/*

***********************************************************************************
* Copyright (C) 2020 - 2021, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "trezorClient.h"
#include "ConnectionManager.h"
#include "trezorDevice.h"
#include "Wallets/SyncWalletsManager.h"
#include "Wallets/SyncHDWallet.h"
#include "ScopeGuard.h"

#include <QNetworkRequest>
#include <QPointer>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QVariant>
#include <QTimer>

TrezorClient::TrezorClient(const std::shared_ptr<spdlog::logger>& logger,
   std::shared_ptr<bs::sync::WalletsManager> walletManager, bool testNet, QObject* parent /*= nullptr*/)
   : QObject(parent)
   , logger_(logger)
   , walletManager_(walletManager)
   , testNet_(testNet)
{
}

QByteArray TrezorClient::getSessionId()
{
   return deviceData_.sessionId_;
}

void TrezorClient::initConnection(bool force, AsyncCallBack&& cb)
{
   auto initCallBack = [this, cbCopy = std::move(cb), force](QNetworkReply* reply) mutable {
      ScopedGuard guard([cb = std::move(cbCopy)]{
         if (cb) {
            cb();
         }
      });
      if (!reply || reply->error() != QNetworkReply::NoError) {
         logger_->error("[TrezorClient] initConnection - Network error: {}", reply->errorString().toUtf8());
         return;
      }

      QByteArray loadData = reply ? reply->readAll().simplified() : "";

      QJsonParseError jsonError;
      QJsonDocument loadDoc = QJsonDocument::fromJson(loadData, &jsonError);
      if (jsonError.error != QJsonParseError::NoError) {
         logger_->error("[TrezorClient] initConnection - Invalid json structure: {}", jsonError.errorString().toUtf8());
         return;
      }

      const auto bridgeInfo = loadDoc.object();
      logger_->info("[TrezorClient] initConnection - Connection initialized. Bridge version: {}"
         , bridgeInfo.value(QString::fromUtf8("version")).toString().toUtf8());

      state_ = State::Init;
      emit initialized();

      enumDevices(force, std::move(guard.releaseCb()));
      reply->deleteLater();
   };

   logger_->info("[TrezorClient] Initialize connection");
   postToTrezor("/", std::move(initCallBack), true);
}

void TrezorClient::initConnection(QString&& deviceId, bool force, AsyncCallBackCall&& cb /*= nullptr*/)
{
   AsyncCallBack cbWrapper = [copyDeviceId = std::move(deviceId), originCb = std::move(cb)]() {
      originCb({ copyDeviceId });
   };

   initConnection(force, std::move(cbWrapper));
}

void TrezorClient::releaseConnection(AsyncCallBack&& cb)
{
   if (deviceData_.sessionId_.isEmpty()) {
      cleanDeviceData();
      if (cb) {
         cb();
      }
      return;
   }

   auto releaseCallback = [this, cbCopy = std::move(cb)](QNetworkReply* reply) mutable {
      ScopedGuard ensureCb([this, cb = std::move(cbCopy)]{
         cleanDeviceData();
         if (cb) {
            cb();
         }
      });

      if (!reply || reply->error() != QNetworkReply::NoError) {
         logger_->error("[TrezorClient] releaseConnection - Network error: {}", reply->errorString().toUtf8());
         return;
      }

      logger_->info("[TrezorClient] releaseConnection - Connection successfully released");

      state_ = State::Released;
      emit deviceReleased();

      reply->deleteLater();
   };

   logger_->info("[TrezorClient] Release connection. Connection id: {}", deviceData_.sessionId_.toStdString());

   QByteArray releaseUrl = "/release/" + deviceData_.sessionId_;
   postToTrezor(std::move(releaseUrl), std::move(releaseCallback));

}

void TrezorClient::postToTrezor(QByteArray&& urlMethod, std::function<void(QNetworkReply*)> &&cb, bool timeout /* = false */)
{
   post(std::move(urlMethod), std::move(cb), QByteArray(), timeout);
}

void TrezorClient::postToTrezorInput(QByteArray&& urlMethod, std::function<void(QNetworkReply*)> &&cb, QByteArray&& input)
{
   post(std::move(urlMethod), std::move(cb), std::move(input));
}

void TrezorClient::call(QByteArray&& input, AsyncCallBackCall&& cb)
{
   auto callCallback = [this, cbCopy = std::move(cb)](QNetworkReply* reply) mutable {

      if (!reply || reply->error() != QNetworkReply::NoError) {
         logger_->error("[TrezorClient] call - Network error: {}", reply->errorString().toUtf8());
         if (cbCopy) {
            cbCopy({});
         }
         return;
      }

      QByteArray loadData = reply->readAll().simplified();
      cbCopy(std::move(loadData));
   };

   logger_->info("[TrezorClient] Call to trezor.");

   QByteArray callUrl = "/call/" + deviceData_.sessionId_;
   postToTrezorInput(std::move(callUrl), std::move(callCallback), std::move(input));
}

QVector<DeviceKey> TrezorClient::deviceKeys() const
{
   if (!trezorDevice_) {
      return {};
   }
   auto key = trezorDevice_->key();
   return { key };
}

QPointer<TrezorDevice> TrezorClient::getTrezorDevice(const QString& deviceId)
{
   // #TREZOR_INTEGRATION: need lookup for several devices
   if (!trezorDevice_) {
      return nullptr;
   }

   assert(trezorDevice_->key().deviceId_ == deviceId);
   return trezorDevice_;
}

void TrezorClient::enumDevices(bool forceAcquire, AsyncCallBack&& cb)
{
   auto enumCallback = [this, cbCopy = std::move(cb), forceAcquire](QNetworkReply* reply) mutable {
      ScopedGuard ensureCb([cb = std::move(cbCopy)]{
         if (cb) {
            cb();
         }
      });

      if (!reply || reply->error() != QNetworkReply::NoError) {
         logger_->error("[TrezorClient] enumDevices - Network error: {}", reply->errorString().toUtf8());
         return;
      }

      QByteArray loadData = reply ? reply->readAll().simplified() : "";

      QJsonParseError jsonError;
      QJsonDocument loadDoc = QJsonDocument::fromJson(loadData, &jsonError);
      if (jsonError.error != QJsonParseError::NoError) {
         logger_->error("[TrezorClient] enumDevices - Invalid json structure: {}", jsonError.errorString().toUtf8());
         return;
      }

      QJsonArray devices = loadDoc.array();
      const int deviceCount = devices.count();
      if (deviceCount == 0) {
         logger_->info("[TrezorClient] enumDevices - No trezor device available");
         return;
      }

      QVector<DeviceData> trezorDevices;
      for (const QJsonValueRef &deviceRef : devices) {
         const QJsonObject deviceObj = deviceRef.toObject();
         trezorDevices.push_back({
            deviceObj[QLatin1String("path")].toString().toUtf8(),
            deviceObj[QLatin1String("vendor")].toString().toUtf8(),
            deviceObj[QLatin1String("product")].toString().toUtf8(),
            deviceObj[QLatin1String("session")].toString().toUtf8(),
            deviceObj[QLatin1String("debug")].toString().toUtf8(),
            deviceObj[QLatin1String("debugSession")].toString().toUtf8() });
      }

      // If there will be a few trezor devices connected, let's choose first one for now
      // later we could expand this functionality to many of them
      if (!forceAcquire && trezorDevice_ && trezorDevices.first().sessionId_ == deviceData_.sessionId_) {
         // this is our previous session so we could go straight away on it
         return;
      }

      deviceData_ = trezorDevices.first();
      logger_->info("[TrezorClient] enumDevices - Enumerate request succeeded. "
         "Total devices available: {}. Trying to acquire first one...", deviceCount);

      state_ = State::Enumerated;
      emit devicesScanned();

      acquireDevice(std::move(ensureCb.releaseCb()));
      reply->deleteLater();
   };

   logger_->info("[TrezorClient] Request to enumerate devices.");
   postToTrezor("/enumerate", std::move(enumCallback));
}

void TrezorClient::acquireDevice(AsyncCallBack&& cb)
{
   QByteArray previousSessionId = deviceData_.sessionId_.isEmpty() ?
      "null" : deviceData_.sessionId_;

   auto acquireCallback = [this, previousSessionId, cbCopy = std::move(cb)](QNetworkReply* reply) mutable {
      ScopedGuard ensureCb([cb = std::move(cbCopy)]{
         if (cb) {
            cb();
         }
      });

      if (!reply || reply->error() != QNetworkReply::NoError) {
         logger_->error("[TrezorClient] acquireDevice - Network error: {}", reply->errorString().toUtf8());
         return;
      }

      QByteArray loadData = reply ? reply->readAll().simplified() : "";
      QJsonObject acuiredDevice = QJsonDocument::fromJson(loadData).object();

      deviceData_.sessionId_ = acuiredDevice[QLatin1String("session")].toString().toUtf8();

      if (deviceData_.sessionId_.isEmpty() || deviceData_.sessionId_ == previousSessionId) {
         logger_->error("[TrezorClient] acquireDevice - Cannot acquire device");
         return;
      }

      logger_->info("[TrezorClient] Connection has successfully acquired. Old "
         "connection id: {}, new connection id: {}", previousSessionId.toStdString()
         , deviceData_.sessionId_.toStdString());

      state_ = State::Acquired;
      emit deviceReady();

      trezorDevice_ = new TrezorDevice(logger_, walletManager_, testNet_, { this }, this) ;
      trezorDevice_->init(std::move(ensureCb.releaseCb()));

      reply->deleteLater();
   };

   logger_->info("[TrezorClient] Acquire new connection. Old connection id: {}", previousSessionId);

   QByteArray acquireUrl = "/acquire/" + deviceData_.path_ + "/" + previousSessionId;
   postToTrezor(std::move(acquireUrl), std::move(acquireCallback));
}

void TrezorClient::post(QByteArray&& urlMethod, std::function<void(QNetworkReply*)> &&cb, QByteArray&& input, bool timeout /* = false*/)
{
   QNetworkRequest request;
   request.setRawHeader({ "Origin" }, { blocksettleOrigin });
   request.setUrl(QUrl(QString::fromLocal8Bit(trezorEndPoint_ + urlMethod)));

   if (!input.isEmpty()) {
      request.setHeader(QNetworkRequest::ContentTypeHeader, { QByteArray("application/x-www-form-urlencoded") });
   }

   QNetworkReply *reply = QNetworkAccessManager().post(request, input);
   auto connection = connect(reply, &QNetworkReply::finished, this
      , [cbCopy = cb, repCopy = reply, sender = QPointer<TrezorClient>(this)]
      {
         if (!sender) {
            return; // TREZOR client already destroyed
         }

         cbCopy(repCopy);
         repCopy->deleteLater();
      });

   // Timeout
   if (timeout) {
      QTimer::singleShot(2000, [replyCopy = QPointer<QNetworkReply>(reply)]() {
         if (!replyCopy) {
            return;
         }
         replyCopy->abort();
      });
   }
}

void TrezorClient::cleanDeviceData()
{
   if (trezorDevice_) {
      trezorDevice_->deleteLater();
      trezorDevice_ = nullptr;
   }
   deviceData_ = {};
}
