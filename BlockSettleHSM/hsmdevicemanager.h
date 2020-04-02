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

#include "hsmcommonstructure.h"
#include "hsmdevicemodel.h"
#include "SecureBinaryData.h"
#include <memory>

#include <QObject>
#include <QVector>
#include <QStringListModel>

class HSMDeviceAbstract;
class TrezorClient;
class LedgerClient;
class ConnectionManager;
namespace bs {
   namespace sync {
      class WalletsManager;
   }
}


class HSMDeviceManager : public QObject
{
   Q_OBJECT
   Q_PROPERTY(HSMDeviceModel* devices READ devices NOTIFY devicesChanged)
   Q_PROPERTY(bool isScanning READ isScanning NOTIFY isScanningChanged)
public:
   HSMDeviceManager(const std::shared_ptr<ConnectionManager>& connectionManager,
      std::shared_ptr<bs::sync::WalletsManager> walletManager, bool testNet, QObject* parent = nullptr);
    ~HSMDeviceManager() override;

   /// Property
   HSMDeviceModel* devices();
   bool isScanning() const;

   ///
   Q_INVOKABLE void scanDevices();
   Q_INVOKABLE void requestPublicKey(int deviceIndex);
   Q_INVOKABLE void setMatrixPin(int deviceIndex, QString pin);
   Q_INVOKABLE void cancel(int deviceIndex);

   Q_INVOKABLE void prepareTrezorForSign(QString deviceId);
   Q_INVOKABLE void signTX(QVariant reqTX);

   Q_INVOKABLE void releaseDevices();

signals:
   void devicesChanged();
   void publicKeyReady(QVariant walletInfo);
   void requestPinMatrix();

   void deviceNotFound(QString deviceId);
   void deviceReady(QString deviceId);
   void deviceTxStatusChanged(QString status);

   void txSigned(SecureBinaryData signData);
   void isScanningChanged();
   void operationFailed();

private:
   void setScanningFlag(bool isScanning);
   void releaseConnection(AsyncCallBack&& cb = nullptr);

   QPointer<HSMDeviceAbstract> getDevice(DeviceKey key);

public:
   std::unique_ptr<TrezorClient> trezorClient_;
   std::unique_ptr<LedgerClient> ledgerClient_;

   HSMDeviceModel* model_;
   bool testNet_{};
   bool isScanning_{};
};

#endif // HSMDEVICESCANNER_H
