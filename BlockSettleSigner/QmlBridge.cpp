#include "QmlBridge.h"



QmlBridge::QmlBridge(const std::shared_ptr<spdlog::logger> &logger, QObject *parent)
   : QObject(parent), logger_(logger)
{

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

//void QmlBridge::execJsCallback(int reqId, const QJSValueList &args)
//{
//   const auto &itCb = cbReqs.find(reqId);
//   if (itCb == cbReqs.end()) {
//      logger_->error("[QmlBridge::{}] failed to find callback for id {}"
//         , __func__, reqId);
//      return;
//   }

//   itCb->second(args);
//   cbReqs.erase(itCb);
//}
