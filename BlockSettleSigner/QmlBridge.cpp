/*

***********************************************************************************
* Copyright (C) 2019 - 2020, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#include "QmlBridge.h"
#include "SignerUiDefs.h"

#include <spdlog/spdlog.h>

// function name in main.qml which invoking other mothods
static const char *kInvokeQmlMethod = "invokeQmlMethod";

// function name in helper.js which calling TX Sign dialog
static const char *kCreateTxSignDialog = "createTxSignDialog";

// function name in helper.js which calling Settlement TX Sign dialog
static const char *kCreateTxSignSettlementDialog = "createTxSignSettlementDialog";

// function name in helper.js which calling various password input dialogs
static const char *kCreatePasswordDialogForType = "createPasswordDialogForType";

// function name in helper.js which displaying control password dialog
static const char *kControlPasswordStatusDialog = "createControlPasswordDialog";

// function name in helper.js which calling general dialogs
static const char *kCustomDialogRequest = "customDialogRequest";

// function name in helper.js which updating settlement dialog
static const char *kUpdateDialogData = "updateDialogData";

// these strings are function names in helper.js which allowed to be evaluated by name
const QList<std::string> qmlCallableDialogMethods =
{
   kControlPasswordStatusDialog,
   kCreateTxSignDialog,
   kCreateTxSignSettlementDialog,
   kCreatePasswordDialogForType,
   kUpdateDialogData
};

const char *QmlBridge::getQmlMethodName(QmlBridge::QmlMethod method)
{
   switch (method) {
   case QmlMethod::CreateTxSignDialog:
      return kCreateTxSignDialog;
   case QmlMethod::CreateTxSignSettlementDialog:
      return kCreateTxSignSettlementDialog;
   case QmlMethod::CreatePasswordDialogForType:
      return kCreatePasswordDialogForType;
   case QmlMethod::ControlPasswordStatusChanged:
      return kControlPasswordStatusDialog;
   case QmlMethod::CustomDialogRequest:
      return kCustomDialogRequest;
   case QmlMethod::UpdateDialogData:
      return kUpdateDialogData;
   }
   return {};
}

QObject *QmlBridge::rootQmlObj() const
{
   return rootQmlObj_;
}

void QmlBridge::setRootQmlObj(QObject *rootQmlObj)
{
   rootQmlObj_ = rootQmlObj;
}

QQmlContext *QmlBridge::ctxt() const
{
   return ctxt_;
}

void QmlBridge::setCtxt(QQmlContext *ctxt)
{
   ctxt_ = ctxt;
}

void QmlBridge::invokeQmlMethod(QmlBridge::QmlMethod method, QmlCallbackBase *cb
    , QVariant val0, QVariant val1, QVariant val2, QVariant val3, QVariant val4, QVariant val5, QVariant val6, QVariant val7) const
{
    if (!qmlCallableDialogMethods.contains(getQmlMethodName(method))) {
        logger_->error("[{}] trying to call qml function which is not allowed: {}", __func__, method);
        return;
    }

    QVariantList argList;
    if (val0.isValid()) argList.append(val0);
    if (val1.isValid()) argList.append(val1);
    if (val2.isValid()) argList.append(val2);
    if (val3.isValid()) argList.append(val3);
    if (val4.isValid()) argList.append(val4);
    if (val5.isValid()) argList.append(val5);
    if (val6.isValid()) argList.append(val6);
    if (val7.isValid()) argList.append(val7);

    QMetaObject::invokeMethod(rootQmlObj_, kInvokeQmlMethod
       , Q_ARG(QVariant, QString::fromLatin1(getQmlMethodName(method)))
       , Q_ARG(QVariant, QVariant::fromValue(cb))
       , Q_ARG(QVariant, QVariant::fromValue(argList)));
}
