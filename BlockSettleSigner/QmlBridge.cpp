#include "QmlBridge.h"
#include "SignerUiDefs.h"

#include <spdlog/spdlog.h>

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

void QmlBridge::invokeQmlMethod(const char *method, QmlCallbackBase *cb
    , QVariant val0, QVariant val1, QVariant val2, QVariant val3, QVariant val4, QVariant val5, QVariant val6, QVariant val7) const
{
    if (!bs::signer::ui::qmlCallableDialogMethods.contains(method)) {
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

    QMetaObject::invokeMethod(rootQmlObj_, "invokeQmlMethod"
       , Q_ARG(QVariant, QString::fromLatin1(method))
       , Q_ARG(QVariant, QVariant::fromValue(cb))
       , Q_ARG(QVariant, QVariant::fromValue(argList)));
}
