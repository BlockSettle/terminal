/*

***********************************************************************************
* Copyright (C) 2019 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef QML_BRIDGE_H
#define QML_BRIDGE_H

#include <QMetaObject>
#include <QObject>
#include <QQmlEngine>

#include "QmlCallbackImpl.h"

#include <memory>

namespace spdlog {
class logger;
}

using namespace bs::signer;

class QmlBridge : public QObject
{
   Q_OBJECT

public:
   QmlBridge(const std::shared_ptr<spdlog::logger> &logger, QObject *parent = nullptr)
      : QObject(parent), logger_(logger) {}

   enum QmlMethod {
      CreateTxSignDialog,
      CreateTxSignSettlementDialog,
      CreatePasswordDialogForType,
      ControlPasswordStatusChanged,
      CustomDialogRequest,
      UpdateDialogData
   };

   static const char *getQmlMethodName(QmlMethod method);

   QObject *rootQmlObj() const;
   void setRootQmlObj(QObject *rootQmlObj);

   QQmlContext *ctxt() const;
   void setCtxt(QQmlContext *ctxt);

   void invokeQmlMethod(QmlMethod method, QmlCallbackBase* cb,
      QVariant val0 = QVariant(), QVariant val1 = QVariant(), QVariant val2 = QVariant(), QVariant val3 = QVariant(),
      QVariant val4 = QVariant(), QVariant val5 = QVariant(), QVariant val6 = QVariant(), QVariant val7 = QVariant()) const;

private:
   QObject        * rootQmlObj_ = nullptr;
   QQmlContext    * ctxt_ = nullptr;
   std::shared_ptr<spdlog::logger> logger_;
};


#endif // QML_BRIDGE_H
