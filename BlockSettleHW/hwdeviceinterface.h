/*

***********************************************************************************
* Copyright (C) 2016 - , BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef HWDEVICEABSTRACT_H
#define HWDEVICEABSTRACT_H

#include "hwcommonstructure.h"
#include <QObject>
#include <QNetworkReply>
#include <QPointer>

class HwDeviceInterface : public QObject
{
   Q_OBJECT

public:
   HwDeviceInterface(QObject* parent = nullptr)
      : QObject(parent) {}
   ~HwDeviceInterface() override = default;

   virtual DeviceKey key() const = 0;
   virtual DeviceType type() const = 0;

   // lifecycle
   virtual void init(AsyncCallBack&& cb = nullptr) = 0;
   virtual void cancel() = 0;
   virtual void clearSession(AsyncCallBack&& cb = nullptr) = 0;

   // operation
   virtual void getPublicKey(AsyncCallBackCall&& cb = nullptr) = 0;
   virtual void signTX(const QVariant& reqTX, AsyncCallBackCall&& cb = nullptr) = 0;

   // Management
   virtual void setMatrixPin(const std::string& pin) {};
   virtual void setPassword(const std::string& password) {};

signals:
   // operation result informing
   void publicKeyReady();
   void deviceTxStatusChanged(QString status);
   void operationFailed(QString reason);

   // Management
   void requestPinMatrix();
   void requestHWPass();
   void canceledOnDevice();
};

#endif // HWDEVICEABSTRACT_H
