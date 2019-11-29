/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef DATABASEEXECUTOR_H
#define DATABASEEXECUTOR_H

#include <memory>
#include <QObject>
#include <memory>

namespace spdlog
{
   class logger;
}

class QSqlQuery;
class QSqlDatabase;

namespace Chat
{
   using LoggerPtr = std::shared_ptr<spdlog::logger>;

   class DatabaseExecutor : public QObject
   {
      Q_OBJECT
   public:
      explicit DatabaseExecutor(QObject* parent = nullptr);

   protected:
      void setLogger(const LoggerPtr& loggerPtr);

      bool PrepareAndExecute(const QString& queryCmd, QSqlQuery& query, const QSqlDatabase& db) const;
      bool checkExecute(QSqlQuery& query) const;

      LoggerPtr loggerPtr_;
   };
}

#endif // DATABASEEXECUTOR_H
