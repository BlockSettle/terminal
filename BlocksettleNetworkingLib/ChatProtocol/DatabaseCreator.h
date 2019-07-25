#ifndef DatabaseCreator_h__
#define DatabaseCreator_h__

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
      DatabaseCreator(const QSqlDatabase& db, const LoggerPtr& loggerPtr, QObject* parent = nullptr);

      void rebuildDatabase();

   signals:
      void rebuildDone();
      void rebuildError();

   private:
      QString buildCreateCmd(const QString& tableName, const TableStructure& structure);
      bool createMissingTables();
      bool checkColumns(const QString& tableName);
      bool ExecuteQuery(const QString& queryCmd, QSqlQuery& query);

      QSqlDatabase db_;
      LoggerPtr loggerPtr_;
      QStringList requiredTables_;
      TablesMap tablesMap_;
   };

   using DatabaseCreatorPtr = std::shared_ptr<DatabaseCreator>;
}

#endif // DatabaseCreator_h__
