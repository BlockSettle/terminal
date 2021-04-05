/*

***********************************************************************************
* Copyright (C) 2020 - 2021, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef TREZORCLIENT_H
#define TREZORCLIENT_H

#include "trezorStructure.h"
#include <memory>

#include <QObject>
#include <QPointer>
#include <QNetworkReply>

class ConnectionManager;
class QNetworkRequest;
class TrezorDevice;

namespace bs {
   namespace sync {
      class WalletsManager;
   }
}

class TrezorClient : public QObject
{
   Q_OBJECT

public:

   TrezorClient(const std::shared_ptr<ConnectionManager>& connectionManager_,
      std::shared_ptr<bs::sync::WalletsManager> walletManager, bool testNet, QObject* parent = nullptr);
   ~TrezorClient() override = default;

   QByteArray getSessionId();

   void initConnection(bool force, AsyncCallBack&& cb = nullptr);
   void initConnection(QString&& deviceId, bool force, AsyncCallBackCall&& cb = nullptr);
   void releaseConnection(AsyncCallBack&& cb = nullptr);

   void call(QByteArray&& input, AsyncCallBackCall&& cb);

   QVector<DeviceKey> deviceKeys() const;
   QPointer<TrezorDevice> getTrezorDevice(const QString& deviceId);

private:
   void postToTrezor(QByteArray&& urlMethod, std::function<void(QNetworkReply*)> &&cb, bool timeout = false);
   void postToTrezorInput(QByteArray&& urlMethod, std::function<void(QNetworkReply*)> &&cb, QByteArray&& input);

   void enumDevices(bool forceAcquire, AsyncCallBack&& cb = nullptr);
   void acquireDevice(AsyncCallBack&& cb = nullptr);
   void post(QByteArray&& urlMethod, std::function<void(QNetworkReply*)> &&cb, QByteArray&& input, bool timeout = false);

   void cleanDeviceData();

signals:
   void initialized();
   void devicesScanned();
   void deviceReady();
   void deviceReleased();

   void publicKeyReady();
   void onRequestPinMatrix();

private:
   std::shared_ptr<ConnectionManager> connectionManager_;
   std::shared_ptr<bs::sync::WalletsManager> walletManager_;

   const QByteArray trezorEndPoint_ = "http://127.0.0.1:21325";
   const QByteArray blocksettleOrigin = "https://blocksettle.trezor.io";
   DeviceData deviceData_;
   State state_ = State::None;
   bool testNet_{};

   // There should really be a bunch of devices
   QPointer<TrezorDevice> trezorDevice_{};


};

#endif // TREZORCLIENT_H
