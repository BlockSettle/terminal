#include "QmlBridge.h"

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
