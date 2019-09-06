#include "DatabaseExecutor.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QThread>

#include <disable_warnings.h>
#include <spdlog/logger.h>
#include <enable_warnings.h>

using namespace Chat;

DatabaseExecutor::DatabaseExecutor(QObject* parent /* = nullptr */) : QObject(parent)
{

}

void DatabaseExecutor::setLogger(const LoggerPtr& loggerPtr)
{
   loggerPtr_ = loggerPtr;
}

bool DatabaseExecutor::PrepareAndExecute(const QString& queryCmd, QSqlQuery& query, const QSqlDatabase& db)
{
   QSqlQuery q(db);
   query = q;

   if (!query.prepare(QLatin1String(queryCmd.toLatin1())))
   {
      loggerPtr_->debug("[DatabaseExecutor::ExecuteQuery] Cannot prepare query: {}", queryCmd.toStdString());
      return false;
   }

   if (!query.exec())
   {
      loggerPtr_->error("[DatabaseExecutor::ExecuteQuery]: Requested query execution error: Query: {}, Error: {}",
         query.executedQuery().toStdString(), query.lastError().text().toStdString());
      return false;
   }

   return true;
}

bool DatabaseExecutor::checkExecute(QSqlQuery& query)
{
   if (!query.exec())
   {
      loggerPtr_->error("[DatabaseExecutor::checkExecute]: Requested query execution error: Query: {}, Error: {}",
         query.executedQuery().toStdString(), query.lastError().text().toStdString());
      return false;
   }

   return true;
}

