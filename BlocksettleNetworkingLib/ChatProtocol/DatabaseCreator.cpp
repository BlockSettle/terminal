#include "DatabaseCreator.h"

#include <set>
#include <QString>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QtDebug>
#include <QThread>

#include <disable_warnings.h>
#include <spdlog/logger.h>
#include <enable_warnings.h>

namespace Chat
{

   DatabaseCreator::DatabaseCreator(const QSqlDatabase& db, const LoggerPtr& loggerPtr, QObject* parent /* = nullptr */) 
      : QObject(parent)
   {
      db_ = db;
      loggerPtr_ = loggerPtr;
   }

   void DatabaseCreator::rebuildDatabase()
   {
      qDebug() << "[DatabaseCreator::rebuildDatabase] thread ID:" << this->thread()->currentThreadId();

      if (createMissingTables())
      {
         emit rebuildDone();
         return;
      }

      emit rebuildError();
   }

   bool DatabaseCreator::createMissingTables()
   {
      const auto& existingTables = db_.tables();
      std::set<QString> tableSet;
      tableSet.insert(existingTables.begin(), existingTables.end());

      bool result = true;
      for (const auto& reqTable : requiredTables_)
      {
         if (tableSet.find(reqTable) == tableSet.end())
         {
            if (!tablesMap_.contains(reqTable))
            {
               loggerPtr_->debug("[DatabaseCreator] Required table '{}' not found in tables description", reqTable.toStdString());
               result = false;
               break;
            }

            QString createCmd = buildCreateCmd(reqTable, tablesMap_.value(reqTable));
            loggerPtr_->debug("[DatabaseCreator] Build create cmd : {}\n", createCmd.toStdString());

            loggerPtr_->debug("[DatabaseCreator] creating table {}", reqTable.toStdString());

            QSqlQuery createQuery;
            const bool rc = ExecuteQuery(createCmd, createQuery);

            if (!rc)
            {
               loggerPtr_->warn("[DatabaseCreator] failed to create table {}", reqTable.toStdString());
            }

            result &= rc;
         }
         else
         {
            loggerPtr_->debug("[DatabaseCreator] table '{}' exists. Checking...", reqTable.toStdString());
            if (!checkColumns(reqTable))
            {
               return false;
            }
         }
      }
      return result;
   }

   QString DatabaseCreator::buildCreateCmd(const QString& tableName, const TableStructure& structure)
   {
      QString cmd = QLatin1String("CREATE TABLE IF NOT EXISTS %1 (%2);");

      QStringList queryParts;

      for (const TableColumnDescription& colDescription : structure.tableColumns)
      {
         QStringList column{ colDescription.columnName, colDescription.columnType };
         queryParts << column.join(QLatin1Char(' '));
      }

      for (const TableForeignKey& foreignKey : structure.foreignKeys)
      {
         QStringList parts {
            QString(QLatin1String("FOREIGN KEY (%1)")).arg(foreignKey.columnKey),
            QString(QLatin1String("REFERENCES %1(%2)"))
                  .arg(foreignKey.foreignTable)
                  .arg(foreignKey.foreignColumn)
         };

         queryParts << parts.join(QLatin1Char(' '));
      }

      for (const TableUniqueCondition& uniqueCondition : structure.uniqueConditions)
      {
         queryParts << QString(QLatin1String("UNIQUE (%1, %2)"))
            .arg(uniqueCondition.firstColumn)
            .arg(uniqueCondition.secondColumn);
      }

      return cmd
         .arg(tableName)
         .arg(queryParts.join(QLatin1String(", ")));
   }

   bool DatabaseCreator::checkColumns(const QString& tableName)
   {
      QString cmd = QString(QLatin1String("DESCRIBE `%1`")).arg(tableName);

      QSqlQuery infoQuery;
      if (!ExecuteQuery(cmd, infoQuery))
      {
         throw std::runtime_error("Can't get table info (table: " + tableName.toStdString() + ")");
         return false;
      }

      QStringList tableColumns;

      while (infoQuery.next())
      {
         tableColumns << infoQuery.value(0).toString();
      }

      const TableStructure& tableStruct = tablesMap_.value(tableName);

      for (const TableColumnDescription& columnItem : tableStruct.tableColumns)
      {
         loggerPtr_->debug("[DatabaseCreator] Check column: {}", columnItem.columnName.toStdString());
         qDebug() << "Check column: " << columnItem.columnName;

         if (!tableColumns.contains(columnItem.columnName))
         {
            loggerPtr_->debug("[DatabaseCreator] Column: {} not exists... Creating with type : {}",
               columnItem.columnName.toStdString(),
               columnItem.columnType.toStdString());

            qDebug() << "Column: " << columnItem.columnName
               << " not exists... Creating with type: " << columnItem.columnType;

            QString alterCmd = QString(QLatin1String("ALTER TABLE `%1` ADD COLUMN "
               " `%2` %3;"))
               .arg(tableName)
               .arg(columnItem.columnName)
               .arg(columnItem.columnType);

            QSqlQuery alterQuery;
            if (!ExecuteQuery(alterCmd, alterQuery))
            {
               qDebug() << "Can't alter table (table: " << columnItem.columnName << ")";
               throw std::runtime_error("Can't alter table (table: " + tableName.toStdString() + ")");
               return false;
            }
         }
         else 
         {
            loggerPtr_->debug("[DatabaseCreator] Column: {}  already exists!", columnItem.columnName.toStdString());
            qDebug() << "Column: " << columnItem.columnName << " already exists!";
         }
      }

      return true;
   }

   bool DatabaseCreator::ExecuteQuery(const QString& queryCmd, QSqlQuery& query)
   {
      QSqlQuery q(db_);
      query = q;

      if (!query.prepare(QLatin1String(queryCmd.toLatin1())))
      {
         loggerPtr_->debug("[DatabaseCreator::ExecuteQuery] Cannot prepare query: {}", queryCmd.toStdString());
         return false;
      }

      if (!query.exec())
      {
         loggerPtr_->error("[DatabaseCreator::ExecuteQuery]: Requested query execution error: Query: {}, Error: {}",
            query.executedQuery().toStdString(), query.lastError().text().toStdString());
         return false;
      }

      return true;
   }

}