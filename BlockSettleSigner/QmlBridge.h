#ifndef QML_BRIDGE_H
#define QML_BRIDGE_H

#include <QMetaObject>
#include <QObject>
#include <QQmlEngine>

#include "ApplicationSettings.h"
#include "QWalletInfo.h"
#include "QSeed.h"
#include "QPasswordData.h"
#include "AuthProxy.h"
#include "ConnectionManager.h"
#include "QmlCallbackImpl.h"
#include <spdlog/spdlog.h>

#include <memory>

using namespace bs::signer;

class QmlBridge : public QObject
{
   Q_OBJECT

public:
   QmlBridge(const std::shared_ptr<spdlog::logger> &logger, QObject *parent = nullptr);


   QObject *rootQmlObj() const;
   void setRootQmlObj(QObject *rootQmlObj);

   QQmlContext *ctxt() const;
   void setCtxt(QQmlContext *ctxt);

   template <typename ...Args>
   void invokeQmlMethod(const char *method, std::shared_ptr<QmlCallback<Args...>> cb,
                        QVariant val0 = QVariant(),
                        QVariant val1 = QVariant(),
                        QVariant val2 = QVariant(),
                        QVariant val3 = QVariant(),
                        QVariant val4 = QVariant(),
                        QVariant val5 = QVariant(),
                        QVariant val6 = QVariant(),
                        QVariant val7 = QVariant())
   {
      reqId++;

      std::function<void(QJSValueList)> serializedCb = [this, cb](const QJSValueList &cbArgs){
         cb->template setJsValues<sizeof...(Args)>(cbArgs);
         cb->exec();
      };

      cbReqs[reqId] = serializedCb;

      QVariantList argList;
      if (val0.isValid()) argList.append(val0);
      if (val1.isValid()) argList.append(val1);
      if (val2.isValid()) argList.append(val2);
      if (val3.isValid()) argList.append(val3);
      if (val4.isValid()) argList.append(val4);
      if (val5.isValid()) argList.append(val5);
      if (val6.isValid()) argList.append(val6);
      if (val7.isValid()) argList.append(val7);

      QMetaObject::invokeMethod(rootQmlObj_, "invokeQmlMetod"
         , Q_ARG(QVariant, QString::fromLatin1(method))
         , Q_ARG(QVariant, QVariant::fromValue(getJsCallback(reqId)))
         , Q_ARG(QVariant, QVariant::fromValue(argList)));
   }

   QJSValue getJsCallback(int reqId)
   {
      QJSValue jsCallback;
      QMetaObject::invokeMethod(rootQmlObj_, "getJsCallback"
         , Q_RETURN_ARG(QJSValue, jsCallback)
         , Q_ARG(QVariant, QVariant::fromValue(reqId)));
      return jsCallback;
   }

   Q_INVOKABLE void execJsCallback(int reqId, const QJSValueList &args);

private:
   QObject        * rootQmlObj_ = nullptr;
   QQmlContext    * ctxt_ = nullptr;
   std::shared_ptr<spdlog::logger> logger_;

   int reqId = 0;
   std::unordered_map<int, std::function<void(QJSValueList args)>> cbReqs;
};


#endif // QML_BRIDGE_H
