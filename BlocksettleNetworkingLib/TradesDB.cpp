#include "TradesDB.h"

#include <set>

#include <QVariant>
#include <QDateTime>
#include <QRandomGenerator>
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
                "   rowid INTEGER PRIMARY KEY ASC,"
                "   name TEXT"
                ");");
                if (!QSqlQuery(db).exec(query)) {
                    return false;
                }
                if (!QSqlQuery(db).exec(QStringLiteral("CREATE INDEX IF NOT EXISTS products_rowid ON products (rowid);"))) {
                    return false;
                }
                if (!QSqlQuery(db).exec(QStringLiteral("CREATE INDEX IF NOT EXISTS products_name ON products (name);"))) {
                    return false;
                }
                return true;
            }
        },

        {
            QStringLiteral("trades"), [db = db_] {
                const QString query = QStringLiteral(
                "CREATE TABLE IF NOT EXISTS trades ("
                "   rowid INTEGER PRIMARY KEY ASC,"
                "   productid INTEGER,"
                "   timestamp REAL,"
                "   price REAL,"
                "   volume REAL,"
                "   FOREIGN KEY(productid) REFERENCES products(rowid)"
                ");");
                if (!QSqlQuery(db).exec(query)) {
                    return false;
                }
                if (!QSqlQuery(db).exec(QStringLiteral("CREATE INDEX IF NOT EXISTS trades_rowid ON trades (rowid);"))) {
                    return false;
                }
                if (!QSqlQuery(db).exec(QStringLiteral("CREATE INDEX IF NOT EXISTS trades_productid ON trades (productid);"))) {
                    return false;
                }
                if (!QSqlQuery(db).exec(QStringLiteral("CREATE INDEX IF NOT EXISTS trades_timestamp ON trades (timestamp);"))) {
                    return false;
                }
                if (!QSqlQuery(db).exec(QStringLiteral("CREATE INDEX IF NOT EXISTS trades_price ON trades (price);"))) {
                    return false;
                }
                if (!QSqlQuery(db).exec(QStringLiteral("CREATE INDEX IF NOT EXISTS trades_volume ON trades (volume);"))) {
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

bool TradesDB::add(const QString& product
                        , const QDateTime& time
                        , const qreal& price
                        , const qreal& volume)
{
    db_.transaction();
    QSqlQuery query(db_);
    query.prepare(QStringLiteral("SELECT rowid FROM products WHERE name = :name;"));
    query.bindValue(QStringLiteral(":name"), product);
    if (!query.exec()) {
        logger_->warn("[TradesDB] failed to get product {}", product.toStdString());
        db_.rollback();
        return false;
    }
    qint64 productid = -1;
    if (query.first()) {
        productid = query.value(QStringLiteral("rowid")).toLongLong();
    } else {
        query.prepare(QStringLiteral("INSERT INTO products(name) VALUES(:name);"));
        query.bindValue(QStringLiteral(":name"), product);
        if (!query.exec()) {
            logger_->warn("[TradesDB] failed to add product {} due {}", product.toStdString(), query.lastError().text().toStdString());
            db_.rollback();
            return false;
        }
        if (!query.lastInsertId().isValid()) {
            logger_->warn("[TradesDB] failed to add product {} due {}", product.toStdString(), query.lastError().text().toStdString());
            db_.rollback();
            return false;
        }
        productid = query.lastInsertId().toLongLong();
    }
    query.prepare(QStringLiteral(
                      "INSERT INTO trades(productid, timestamp, price, volume) "
                      "VALUES(:productid, :timestamp, :price, :volume);"));
    query.bindValue(QStringLiteral(":productid"), productid);
    query.bindValue(QStringLiteral(":timestamp"), time.toMSecsSinceEpoch());
    query.bindValue(QStringLiteral(":price"), price);
    query.bindValue(QStringLiteral(":volume"), volume);
    if (!query.exec()) {
        logger_->warn("[TradesDB] failed to add trade {}", product.toStdString());
        db_.rollback();
        return false;
    }
    db_.commit();
    return true;
}

void TradesDB::init()
{
    populateEmptyData();
}

TradesDB::DataPoint *TradesDB::getDataPoint(const QString &product
                                 , const QDateTime &timeTill
                                 , qint64 durationSec)
{
    db_.transaction();
    QSqlQuery query(db_);
    query.prepare(QStringLiteral("SELECT rowid FROM products WHERE name = :name;"));
    query.bindValue(QStringLiteral(":name"), product);
    if (!query.exec() || !query.first()) {
        logger_->warn("[TradesDB] failed find product {}", product.toStdString());
        db_.rollback();
        return nullptr;
    }
    qint64 productid = query.value(QStringLiteral("rowid")).toLongLong();
    QDateTime timeSince = timeTill.addSecs(-durationSec);
    query.prepare(QStringLiteral("SELECT u.price as open, "
                                 "    MAX(t.price) AS high, "
                                 "    MIN(t.price) AS low, "
                                 "    w.price AS close, "
                                 "    SUM(t.volume) AS volume "
                                 "FROM ( "
                                 "    SELECT * "
                                 "    FROM trades "
                                 "    WHERE productid = :productid AND "
                                 "        timestamp > :timeSince AND "
                                 "        timestamp <= :timeTill) AS t JOIN ( "
                                 "    SELECT price "
                                 "    FROM trades "
                                 "    WHERE productid = :productid AND "
                                 "        timestamp > :timeSince AND "
                                 "        timestamp <= :timeTill "
                                 "    ORDER BY timestamp ASC "
                                 "    LIMIT 1) AS u JOIN ( "
                                 "    SELECT price "
                                 "    FROM trades "
                                 "    WHERE productid = :productid AND "
                                 "        timestamp > :timeSince AND "
                                 "        timestamp <= :timeTill "
                                 "    ORDER BY timestamp DESC "
                                 "    LIMIT 1) AS w ; "));
    query.bindValue(QStringLiteral(":productid"), productid);
    query.bindValue(QStringLiteral(":timeSince") , timeSince.toMSecsSinceEpoch());
    qreal timestamp = timeTill.toMSecsSinceEpoch();
    query.bindValue(QStringLiteral(":timeTill") , timestamp);
    if (!query.exec() || !query.first()) {
        logger_->warn("[TradesDB] failed read data: {} : {} using {} {} {}"
                      , query.lastError().text().toStdString()
                      , query.lastQuery().toStdString()
                      , productid
                      , timeSince.toMSecsSinceEpoch()
                      , timestamp);
        db_.rollback();
        return nullptr;
    }
    db_.commit();
    qreal open = query.value(QStringLiteral("open")).toDouble();
    qreal high = query.value(QStringLiteral("high")).toDouble();
    qreal low = query.value(QStringLiteral("low")).toDouble();
    qreal close = query.value(QStringLiteral("close")).toDouble();
    qreal volume = query.value(QStringLiteral("volume")).toDouble();
    return new DataPoint { .open = open
                , .high = high
                , .low = low
                , .close = close
                , .volume = volume
                , .timestamp = timestamp };
}

const std::vector<TradesDB::DataPoint *> TradesDB::getDataPoints(
        const QString &product
        , const QDateTime &sinceTime
        , const QDateTime &tillTime
        , qint64 stepDurationSecs)
{
    std::vector<TradesDB::DataPoint *> result;
    QDateTime end = tillTime;
    while (end > sinceTime) {
        logger_->info("[TradesDB] get point for {} {} {}"
                      , product.toStdString()
                      , end.toString(Qt::ISODate).toStdString()
                      , stepDurationSecs);
        auto data = getDataPoint(product, end, stepDurationSecs);
        if (data) {
            result.push_back(data);
        }
        end = end.addSecs(-stepDurationSecs);
    }
    return result;
}

bool TradesDB::populateEmptyData()
{
    QSqlQuery query(db_);
    if (!query.exec(QStringLiteral("SELECT * FROM trades;"))) {
        logger_->warn("[TradesDB] failed to count trades");
        return false;
    }
    if (query.first()) {
        return false;
    }
    QStringList products = {
        QStringLiteral("EUR/GBP")
        , QStringLiteral("EUR/JPY")
        , QStringLiteral("EUR/SEK")
        , QStringLiteral("GBP/JPY")
        , QStringLiteral("GBP/SEK")
        , QStringLiteral("JPY/SEK")
    };
    QDateTime time = QDateTime::currentDateTime().addYears(-1);
    QDateTime now = QDateTime::currentDateTime();
    int j = 0;
    while (time < now) {
        int i = QRandomGenerator::global()->bounded(0, products.count());
        const QString product = products.at(i);
        qreal price = 100 + QRandomGenerator::global()->generateDouble() * 200;
        qreal volume = QRandomGenerator::global()->generateDouble() * 200;
        time = time.addMSecs(3550000 + QRandomGenerator::global()->generateDouble() * 100000);
        if (j != time.date().month() && time.date().day() == 1) {
            j = time.date().month();
            logger_->info("[TradesDB] generate since {}", time.toString(Qt::ISODate).toStdString());
        }
        add(product, time, price, volume);
    }
    return true;
}
