/*

***********************************************************************************
* Copyright (C) 2016 - , BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef HSMDEVICESCANNER_H
#define HSMDEVICESCANNER_H

#include "trezor/trezorStructure.h"
#include "SecureBinaryData.h"
#include <memory>

#include <QObject>
#include <QVector>
#include <QStringListModel>

class TrezorClient;
class ConnectionManager;
namespace bs {
   namespace sync {
      class WalletsManager;
   }
}


class HSMDeviceManager : public QObject
{
   Q_OBJECT
   Q_PROPERTY(QStringListModel* devices READ devices NOTIFY devicesChanged)
public:
   HSMDeviceManager(const std::shared_ptr<ConnectionManager>& connectionManager,
      std::shared_ptr<bs::sync::WalletsManager> walletManager, bool testNet, QObject* parent = nullptr);
    ~HSMDeviceManager() override;

   // Property
   QStringListModel* devices();

   Q_INVOKABLE void scanDevices();
   Q_INVOKABLE void requestPublicKey(int deviceIndex);
   Q_INVOKABLE void setMatrixPin(int deviceIndex, QString pin);
   Q_INVOKABLE void cancel(int deviceIndex);

   Q_INVOKABLE void prepareTrezorForSign(QString deviceId);
   Q_INVOKABLE void signTX(QVariant reqTX);

   Q_INVOKABLE void releaseDevices();

signals:
   void devicesChanged();
   void publicKeyReady(QString xpubNested, QString xpubNative, QString label, QString vendor);
   void requestPinMatrix();

   void deviceNotFound(QString deviceId);
   void deviceReady(QString deviceId);
   void deviceTxStatusChanged(QString status);

   void txSigned(SecureBinaryData signData);

public:
   std::unique_ptr<TrezorClient> trezorClient_;
   QVector<DeviceKey> devices_;
   QStringListModel* model_;
   bool testNet_{};
};

#endif // HSMDEVICESCANNER_H
