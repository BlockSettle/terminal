#ifndef TRADESDB_H
#define TRADESDB_H

#include <QObject>
#include <QtSql/QSqlDatabase>

#include <memory>
#include <functional>

#include "qt_ext.h"

namespace spdlog {
   class logger;
}

class TradesDB : public QObject
{
   Q_OBJECT
   Q_DISABLE_COPY_X(TradesDB)
public:
   explicit TradesDB(const std::shared_ptr<spdlog::logger> &logger
                     , const QString &dbFile
                     , QObject *parent = nullptr);
   ~TradesDB() override;

   /**
    * @brief Check if order already marked as filled
    * @param orderId
    * @param timestamp
    * @param settlementDate
    * @param save Do update DB after check?
    * @return True if order already marked as filled, false otherwise
    */
   bool checkOrder(qint64 orderId
                   , qint64 timestamp
                   , const QString &settlementDate
                   , bool save = true);

private:
   bool createMissingTables();
   bool saveOrder(qint64 orderId
                  , qint64 timestamp
                  , const QString &settlementDate);
   bool removeOrder(qint64 orderId);
   bool removeOrdersBefore(qint64 timestamp);

private slots:
   void cleanOldFilledOrders();

private:
   std::shared_ptr<spdlog::logger>  logger_;
   QSqlDatabase                     db_;
   const QStringList                requiredTables_;

   typedef std::function<bool(void)> TableCreator;
   std::map<QString, TableCreator>  tableCreators_;
};

#endif // TRADESDB_H
