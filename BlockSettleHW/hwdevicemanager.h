/*

***********************************************************************************
* Copyright (C) 2016 - , BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef HWDEVICESCANNER_H
#define HWDEVICESCANNER_H

#include "hwcommonstructure.h"
#include "hwdevicemodel.h"
#include "SecureBinaryData.h"
#include <memory>

#include <QObject>
#include <QVector>
#include <QStringListModel>

class HwDeviceInterface;
class TrezorClient;
class LedgerClient;
class ConnectionManager;
namespace bs {
   namespace sync {
      class WalletsManager;
   }
}


class HwDeviceManager : public QObject
{
   Q_OBJECT
   Q_PROPERTY(HwDeviceModel* devices READ devices NOTIFY devicesChanged)
   Q_PROPERTY(bool isScanning READ isScanning NOTIFY isScanningChanged)

public:
   HwDeviceManager(const std::shared_ptr<ConnectionManager>& connectionManager,
      std::shared_ptr<bs::sync::WalletsManager> walletManager, bool testNet, QObject* parent = nullptr);
    ~HwDeviceManager() override;

   /// Property
   HwDeviceModel* devices();
   bool isScanning() const;

   // Actions from UI
   Q_INVOKABLE void scanDevices();
   Q_INVOKABLE void requestPublicKey(int deviceIndex);
   Q_INVOKABLE void setMatrixPin(int deviceIndex, QString pin);
   Q_INVOKABLE void setPassphrase(int deviceIndex, QString passphrase);
   Q_INVOKABLE void cancel(int deviceIndex);
   Q_INVOKABLE void prepareHwDeviceForSign(QString walletId);
   Q_INVOKABLE void signTX(QVariant reqTX);
   Q_INVOKABLE void releaseDevices();

   // Info asked from UI
   Q_INVOKABLE bool awaitingUserAction(int deviceIndex);
   Q_INVOKABLE QString lastDeviceError();

signals:
   void devicesChanged();
   void publicKeyReady(QVariant walletInfo);
   void requestPinMatrix();
   void requestHWPass();

   void deviceNotFound(QString deviceId);
   void deviceReady(QString deviceId);
   void deviceTxStatusChanged(QString status);

   void txSigned(SecureBinaryData signData);
   void isScanningChanged();
   void operationFailed(QString reason);
   void cancelledOnDevice();

private:
   void setScanningFlag(bool isScanning);
   void releaseConnection(AsyncCallBack&& cb = nullptr);
   void scanningDone();

   QPointer<HwDeviceInterface> getDevice(DeviceKey key);

public:
   std::unique_ptr<TrezorClient> trezorClient_;
   std::unique_ptr<LedgerClient> ledgerClient_;
   std::shared_ptr<bs::sync::WalletsManager> walletManager_;

   HwDeviceModel* model_;
   bool testNet_{};
   bool isScanning_{};
   bool isSigning_{};
   QString lastOperationError_;
};

#endif // HWDEVICESCANNER_H
