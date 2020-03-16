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
#include <QObject>
#include <QVector>
#include <QStringListModel>

class TrezorClient;
class ConnectionManager;

class HSMDeviceManager : public QObject
{
   Q_OBJECT
   Q_PROPERTY(QStringListModel* devices READ devices NOTIFY devicesChanged)
public:
   HSMDeviceManager(const std::shared_ptr<ConnectionManager>& connectionManager, QObject* parent = nullptr);
    ~HSMDeviceManager() override = default;

   // Property
   QStringListModel* devices();

   Q_INVOKABLE void scanDevices();
   Q_INVOKABLE void requestPublicKey(int deviceIndex);
   Q_INVOKABLE void setMatrixPin(int deviceIndex, QString pin);
   Q_INVOKABLE void cancel(int deviceIndex);

signals:
   void devicesChanged();
   void publicKeyReady();
   void requestPinMatrix();

public:
   std::unique_ptr<TrezorClient> trezorClient_{};
   QVector<DeviceKey> devices_;
   QStringListModel* model_;
};

#endif // HSMDEVICESCANNER_H
