#include "TradesDB.h"

#include <QVariant>
#include <QTimer>
#include <QDateTime>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlDriver>
#include <QtSql/QSqlError>

#include <spdlog/spdlog.h>

const qint64 kMaxDaysKeepFilledOrders = 3;

const QString kTableFilledOrders = QStringLiteral("filled_orders");

TradesDB::TradesDB(const std::shared_ptr<spdlog::logger> &logger
                   , const QString &dbFile
                   , QObject *parent)
   : QObject(parent)
   , logger_(logger)
   , db_(QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("trades")))
   , requiredTables_({ kTableFilledOrders })
{
   db_.setDatabaseName(dbFile);

   if (!db_.open()) {
      throw std::runtime_error("failed to open " + db_.connectionName().toStdString()
                               + " DB: " + db_.lastError().text().toStdString());
   }

   tableCreators_ = {
      { kTableFilledOrders, [db = db_]() -> bool {
           const QString query = QStringLiteral("CREATE TABLE IF NOT EXISTS %1 ( "
           "   order_id INTEGER PRIMARY KEY, "
           "   timestamp INTEGER, "
           "   settlement_date TEXT "
           ");").arg(kTableFilledOrders);
           if (!QSqlQuery(db).exec(query)) {
              return false;
           }
           const QString queryIndexOrderId = QStringLiteral("CREATE UNIQUE INDEX %1_order_id "
           "   ON %1(order_id);").arg(kTableFilledOrders);
           if (!QSqlQuery(db).exec(queryIndexOrderId)) {
              return false;
           }
           const QString queryIndexTimestamp = QStringLiteral("CREATE INDEX %1_timestamp "
           "   ON %1(timestamp);").arg(kTableFilledOrders);
           if (!QSqlQuery(db).exec(queryIndexTimestamp)) {
              return false;
           }
           const QString queryIndexSettlementDate = QStringLiteral("CREATE INDEX %1_settlement_date "
           "   ON %1(settlement_date);").arg(kTableFilledOrders);
           if (!QSqlQuery(db).exec(queryIndexSettlementDate)) {
              return false;
           }
           return true;
        }
      }
   };

   if (!createMissingTables()) {
      throw std::runtime_error("failed to create tables in " + db_.connectionName().toStdString() + " DB");
   }

   QTimer::singleShot(0, this, &TradesDB::cleanOldFilledOrders);
}

TradesDB::~TradesDB() = default;

bool TradesDB::checkOrder(qint64 orderId
                          , qint64 timestamp
                          , const QString &settlementDate
                          , bool save)
{
   const QString query = QStringLiteral("SELECT * "
                                        "FROM %1 "
                                        "WHERE order_id = :order_id "
                                        "LIMIT 1;").arg(kTableFilledOrders);
   QSqlQuery q(db_);
   q.prepare(query);
   q.bindValue(QStringLiteral(":order_id"), orderId);
   if (!q.exec()) {
      logger_->warn("[TradesDB] failed to check order exist {}", q.lastError().text().toStdString());
      return false;
   }
   const bool result = q.first();
   if (!result) {
      if (save) {
         logger_->debug("[TradesDB] need to save order id: {}", orderId);
         saveOrder(orderId, timestamp, settlementDate);
      }
   }
   return result;
}

bool TradesDB::createMissingTables()
{
   const auto existingTables = db_.tables();

   bool result = true;
   db_.transaction();
   for (const QString &table : requiredTables_) {
      if (!existingTables.contains(table)) {
         logger_->debug("[TradesDB] creating table {}", table.toStdString());
         const bool res = tableCreators_[table]();
         if (!res) {
            logger_->warn("[TradesDB] failed to create table {}", table.toStdString());
         }
         result &= res;
      }
   }
   if (result) {
      db_.commit();
   } else {
      db_.rollback();
   }
   return result;
}

bool TradesDB::saveOrder(qint64 orderId, qint64 timestamp, const QString &settlementDate)
{
   const QString query = QStringLiteral("INSERT INTO %1 ( "
                                        "  order_id, "
                                        "  timestamp, "
                                        "  settlement_date "
                                        ") VALUES ( "
                                        "  :order_id, "
                                        "  :timestamp, "
                                        "  :settlement_date "
                                        ");").arg(kTableFilledOrders);
   QSqlQuery q(db_);
   q.prepare(query);
   q.bindValue(QStringLiteral(":order_id"), orderId);
   q.bindValue(QStringLiteral(":timestamp"), timestamp);
   q.bindValue(QStringLiteral(":settlement_date"), settlementDate);
   if (!q.exec()) {
      logger_->warn("[TradesDB] failed to insert new order {}", q.lastError().text().toStdString());
      return false;
   }
   const bool result = q.lastInsertId().isValid();
   if (!result) {
      logger_->warn("[TradesDB] failed to insert new order {} {} {}", orderId, timestamp, settlementDate.toStdString());
   }
   return result;
}

bool TradesDB::removeOrder(qint64 orderId)
{
   const QString query = QStringLiteral("DELETE FROM %1 "
                                        "WHERE order_id = :order_id;").arg(kTableFilledOrders);
   QSqlQuery q(db_);
   q.prepare(query);
   q.bindValue(QStringLiteral(":order_id"), orderId);
   if (!q.exec()) {
      logger_->warn("[TradesDB] failed to remove order {}", q.lastError().text().toStdString());
      return false;
   }
   const bool result = q.numRowsAffected() > 0;
   if (!result) {
      logger_->warn("[TradesDB] failed to remove order {}", orderId);
   }
   return result;
}

bool TradesDB::removeOrdersBefore(qint64 timestamp)
{
   const QString query = QStringLiteral("DELETE FROM %1 "
                                        "WHERE timestamp < :timestamp;").arg(kTableFilledOrders);
   QSqlQuery q(db_);
   q.prepare(query);
   q.bindValue(QStringLiteral(":timestamp"), timestamp);
   if (!q.exec()) {
      logger_->warn("[TradesDB] failed to remove orders {}", q.lastError().text().toStdString());
      return false;
   }
   const bool result = q.numRowsAffected() > 0;
   if (!result) {
      logger_->warn("[TradesDB] failed to remove orders before {}", timestamp);
   }
   return result;
}

void TradesDB::cleanOldFilledOrders()
{
   // Remove filled order info older than N days ago
   auto timestamp = QDateTime::currentDateTimeUtc()
         .addDays(-kMaxDaysKeepFilledOrders)
         .toSecsSinceEpoch();
   removeOrdersBefore(timestamp);
}
