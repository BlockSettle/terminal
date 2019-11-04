#ifndef DATABASECREATOR_H
#define DATABASECREATOR_H

#include <memory>
#include <QObject>
#include <QSqlDatabase>
#include <QMap>

namespace spdlog
{
   class logger;
}

namespace Chat
{
   struct TableColumnDescription {
      QString columnName;
      QString columnType;
   };

   struct TableForeignKey {
      QString columnKey;
      QString foreignTable;
      QString foreignColumn;
      QString foreignReferentialAction;
   };

   struct TableUniqueCondition {
      QString firstColumn;
      QString secondColumn;
   };

   struct TableStructure {
      QList<TableColumnDescription> tableColumns;
      QList<TableForeignKey> foreignKeys = {};
      QList<TableUniqueCondition> uniqueConditions = {};
   };

   using LoggerPtr = std::shared_ptr<spdlog::logger>;
   using TablesMap = QMap<QString, TableStructure>;

   class DatabaseCreator : public QObject
   {
      Q_OBJECT
   public:
      explicit DatabaseCreator(const QSqlDatabase& db, const LoggerPtr& loggerPtr, QObject* parent = nullptr);

      virtual void rebuildDatabase();

   signals:
      void rebuildDone();
      void rebuildError();

   protected:
      QStringList requiredTables_;
      TablesMap tablesMap_;

   private:
      static QString buildCreateCmd(const QString& tableName, const TableStructure& structure);
      bool createMissingTables();
      bool checkColumns(const QString& tableName) const;
      bool ExecuteQuery(const QString& queryCmd, QSqlQuery& query) const;

      QSqlDatabase db_;
      LoggerPtr loggerPtr_;
   };

   using DatabaseCreatorPtr = std::shared_ptr<DatabaseCreator>;
}

#endif // DATABASECREATOR_H
