#include "TradesDB.h"

#include <set>

#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>

#include <spdlog/spdlog.h>

TradesDB::TradesDB(const std::shared_ptr<spdlog::logger> &logger,
                   const QString &dbFile,
                   QObject *parent)
    : QObject(parent)
    , logger_(logger)
    , requiredTables_({QStringLiteral("products"), QStringLiteral("trades")})
{
    db_ = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("trades"));
    db_.setDatabaseName(dbFile);

    if (!db_.open()) {
       throw std::runtime_error("failed to open " + db_.connectionName().toStdString()
          + " DB: " + db_.lastError().text().toStdString());
    }

    createTable_ = {
        {
            QStringLiteral("products"), [db = db_] {
                const QString query = QStringLiteral(
                "CREATE TABLE IF NOT EXISTS products ("
                "rowid INTEGER PRIMARY KEY ASC,"
                "name TEXT"
                ");");
                if (!QSqlQuery(db).exec(query)) {
                    return false;
                }
                return true;
            }
        },

        {
            QStringLiteral("trades"), [db = db_] {
                const QString query = QStringLiteral(
                "CREATE TABLE IF NOT EXISTS trades ("
                "rowid INTEGER PRIMARY KEY ASC,"
                "productid INTEGER,"
                "timestamp REAL,"
                "price REAL,"
                "volume REAL,"
                "FOREIGN KEY(productid) REFERENCES products(rowid)"
                ");");
                if (!QSqlQuery(db).exec(query)) {
                    return false;
                }
                return true;
            }
        }
    };
    if (!createMissingTables()) {
       throw std::runtime_error("failed to create tables in " + db_.connectionName().toStdString() + " DB");
    }
}

bool TradesDB::createMissingTables()
{
    const auto &existingTables = db_.tables();
    std::set<QString> tableSet;
    tableSet.insert(existingTables.begin(), existingTables.end());

    bool result = true;
    for (const auto &reqTable : requiredTables_) {
       if (tableSet.find(reqTable) == tableSet.end()) {
          logger_->debug("[TradesDB] creating table {}", reqTable.toStdString());
          const bool rc = createTable_[reqTable]();
          if (!rc) {
             logger_->warn("[TradesDB] failed to create table {}", reqTable.toStdString());
          }
          result &= rc;
       }
    }
    return result;
}
