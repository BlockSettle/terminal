
#include "UserScriptRunner.h"

#include <QThread>


//
// UserScriptHandler
//

UserScriptHandler::UserScriptHandler(std::shared_ptr<QuoteProvider> quoteProvider,
   UserScriptRunner *runner)
{
   connect(quoteProvider.get(), &QuoteProvider::quoteReqNotifReceived,
      this, &UserScriptHandler::onQuoteReqNotification, Qt::QueuedConnection);
   connect(quoteProvider.get(), &QuoteProvider::quoteNotifCancelled,
      this, &UserScriptHandler::onQuoteReqCancelled, Qt::QueuedConnection);
   connect(runner, &UserScriptRunner::initAQ, this, &UserScriptHandler::initAQ,
      Qt::QueuedConnection);
   connect(runner, &UserScriptRunner::deinitAQ, this, &UserScriptHandler::deinitAQ,
      Qt::QueuedConnection);
}

void UserScriptHandler::onQuoteReqNotification(const bs::network::QuoteReqNotification &qrn)
{

}

void UserScriptHandler::onQuoteReqCancelled(const QString &reqId, bool byUser)
{

}

void UserScriptHandler::initAQ(const QString &fileName)
{

}

void UserScriptHandler::deinitAQ()
{

}


//
// UserScriptRunner
//

UserScriptRunner::UserScriptRunner(std::shared_ptr<QuoteProvider> quoteProvider, QObject *parent)
   : QObject(parent)
   , thread_(new QThread(this))
   , script_(new UserScriptHandler(quoteProvider, this))
   , enabled_(false)
{
   script_->moveToThread(thread_);
}

bool UserScriptRunner::isEnabled() const
{
   return enabled_;
}

void UserScriptRunner::enableAQ(const QString &fileName)
{

}

void UserScriptRunner::disableAQ()
{

}
