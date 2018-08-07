
#ifndef USERSCRIPTRUNNER_H_INCLUDED
#define USERSCRIPTRUNNER_H_INCLUDED

#include <QObject>

#include <unordered_map>
#include <memory>
#include <string>

#include "UserScript.h"
#include "TransactionData.h"
#include "QuoteProvider.h"

QT_BEGIN_NAMESPACE
class QThread;
QT_END_NAMESPACE

class UserScriptRunner;


//
// UserScriptHandler
//

//! Handler of events in user script.
class UserScriptHandler : public QObject
{
   Q_OBJECT

public:
   explicit UserScriptHandler(std::shared_ptr<QuoteProvider> quoteProvider,
      UserScriptRunner *runner);
   ~UserScriptHandler() noexcept override = default;

private slots:
   void onQuoteReqNotification(const bs::network::QuoteReqNotification &qrn);
   void onQuoteReqCancelled(const QString &reqId, bool byUser);
   void initAQ(const QString &fileName);
   void deinitAQ();

private:
   AutoQuoter *aq_;
   std::unordered_map<std::string, QObject*> aqObjs_;
   std::unordered_map<std::string, bs::network::QuoteReqNotification> aqQuoteReqs_;
   std::unordered_map<std::string, std::shared_ptr<TransactionData>> aqTxData_;
}; // class UserScriptHandler


//
// UserScriptRunner
//

//! Runner of user script.
class UserScriptRunner : public QObject
{
   Q_OBJECT

signals:
   void initAQ(const QString &fileName);
   void deinitAQ();
   void stateChanged(bool enabled);

public:
   UserScriptRunner(std::shared_ptr<QuoteProvider> quoteProvider, QObject *parent);
   ~UserScriptRunner() noexcept override = default;

   bool isEnabled() const;

public slots:
   void enableAQ(const QString &fileName);
   void disableAQ();

private:
   QThread *thread_;
   UserScriptHandler *script_;
   bool enabled_;
}; // class UserScriptRunner

#endif // USERSCRIPTRUNNER_H_INCLUDED
