#include "TradesDB.h"

#include <QtSql/QSqlError>

TradesDB::TradesDB(const std::shared_ptr<spdlog::logger> &logger
                   , const QString &dbFile
                   , QObject *parent)
   : QObject(parent)
   , logger_(logger)
   , db_(QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("trades")))
{
   db_.setDatabaseName(dbFile);

   if (!db_.open()) {
      throw std::runtime_error("failed to open " + db_.connectionName().toStdString()
                               + " DB: " + db_.lastError().text().toStdString());
   }
}

bool TradesDB::checkOrder(qint64 orderId
                          , qint64 timestamp
                          , const QString &settlementDate, bool save)
{
   return false;
}

TradesDB::~TradesDB() = default;
