#ifndef QML_BRIDGE_H
#define QML_BRIDGE_H

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
                        QGenericArgument val0 = QGenericArgument(nullptr),
                        QGenericArgument val1 = QGenericArgument(),
                        QGenericArgument val2 = QGenericArgument(),
                        QGenericArgument val3 = QGenericArgument(),
                        QGenericArgument val4 = QGenericArgument(),
                        QGenericArgument val5 = QGenericArgument(),
                        QGenericArgument val6 = QGenericArgument(),
                        QGenericArgument val7 = QGenericArgument(),
                        QGenericArgument val8 = QGenericArgument())
   {
      reqId++;

      std::function<void(QJSValueList)> serializedCb = [this, cb](const QJSValueList &cbArgs){
         cb->template setJsValues<sizeof...(Args)>(cbArgs);
         cb->exec();
      };

      cbReqs[reqId] = serializedCb;
   }

   Q_INVOKABLE void execJsCallback(int reqId, QJSValueList args);

private:
   QObject        * rootQmlObj_ = nullptr;
   QQmlContext    * ctxt_ = nullptr;
   std::shared_ptr<spdlog::logger> logger_;

   int reqId = 0;
   std::unordered_map<int, std::function<void(QJSValueList args)>> cbReqs;
};


#endif // QML_BRIDGE_H
