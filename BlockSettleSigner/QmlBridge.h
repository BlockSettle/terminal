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

namespace  {
   // these strings are function names in helper.js which can be evaluated by name
   const QList<std::string> knownMethods =
   {
      "createTxSignDialog",
      "createSettlementTransactionDialog",
      "updateDialogData",
      "createPasswordDialogForType"
   };
}

using namespace bs::signer;

class QmlBridge : public QObject
{
   Q_OBJECT

public:
   QmlBridge(const std::shared_ptr<spdlog::logger> &logger, QObject *parent = nullptr)
      : QObject(parent), logger_(logger) {}

   QObject *rootQmlObj() const;
   void setRootQmlObj(QObject *rootQmlObj);

   QQmlContext *ctxt() const;
   void setCtxt(QQmlContext *ctxt);

   void invokeQmlMethod(const char *method, QmlCallbackBase* cb,
                        QVariant val0 = QVariant(),
                        QVariant val1 = QVariant(),
                        QVariant val2 = QVariant(),
                        QVariant val3 = QVariant(),
                        QVariant val4 = QVariant(),
                        QVariant val5 = QVariant(),
                        QVariant val6 = QVariant(),
                        QVariant val7 = QVariant()) const
   {
      if (!knownMethods.contains(method)) {
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

private:
   QObject        * rootQmlObj_ = nullptr;
   QQmlContext    * ctxt_ = nullptr;
   std::shared_ptr<spdlog::logger> logger_;
};


#endif // QML_BRIDGE_H
