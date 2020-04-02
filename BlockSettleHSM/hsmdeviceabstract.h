/*

***********************************************************************************
* Copyright (C) 2016 - , BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef HSMDEVICEABSTRACT_H
#define HSMDEVICEABSTRACT_H

#include "hsmcommonstructure.h"
#include <QObject>
#include <QNetworkReply>
#include <QPointer>

class HSMDeviceAbstract : public QObject
{
   Q_OBJECT

public:
   HSMDeviceAbstract(QObject* parent = nullptr)
      : QObject(parent) {}
   ~HSMDeviceAbstract() override = default;

   virtual DeviceKey key() const = 0;
   virtual DeviceType type() const = 0;

   // lifecycle
   virtual void init(AsyncCallBack&& cb = nullptr) = 0;
   virtual void cancel() = 0;

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
   void operationFailed();

   // Management
   void requestPinMatrix();
   void requestHSMPass();
};

#endif // HSMDEVICEABSTRACT_H
