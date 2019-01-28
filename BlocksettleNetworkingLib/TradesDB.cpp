#include "TradesDB.h"

#include <set>

#include <QVariant>
#include <QDateTime>
#include <QRandomGenerator>
#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>

#include <spdlog/spdlog.h>

namespace {

static const QString ONE_YEAR = QStringLiteral("1Y");
static const QString SIX_MONTHS = QStringLiteral("6M");
static const QString ONE_MONTH = QStringLiteral("1M");
static const QString ONE_WEEK = QStringLiteral("1W");
static const QString TWENTY_FOUR_HOURS = QStringLiteral("24H");
static const QString TWELVE_HOURS = QStringLiteral("12H");
static const QString SIX_HOURS = QStringLiteral("6H");
static const QString ONE_HOUR = QStringLiteral("1H");
static const QString THIRTY_MINUTES = QStringLiteral("30m");
static const QString FIFTEEN_MINUTES = QStringLiteral("15m");
static const QString ONE_MINUTE = QStringLiteral("1m");

static const std::map<TradesDB::Interval, QString> INTERVALS_LABELS = {
    {
        TradesDB::Interval::OneYear         , ONE_YEAR
    }, {
        TradesDB::Interval::SixMonths       , SIX_MONTHS
    }, {
        TradesDB::Interval::OneMonth        , ONE_MONTH
    }, {
        TradesDB::Interval::OneWeek         , ONE_WEEK
    }, {
        TradesDB::Interval::TwentyFourHours , TWENTY_FOUR_HOURS
    }, {
        TradesDB::Interval::TwelveHours     , TWELVE_HOURS
    }, {
        TradesDB::Interval::SixHours        , SIX_HOURS
    }, {
        TradesDB::Interval::OneHour         , ONE_HOUR
    }, {
        TradesDB::Interval::ThirtyMinutes   , THIRTY_MINUTES
    }, {
        TradesDB::Interval::FifteenMinutes  , FIFTEEN_MINUTES
    }, {
        TradesDB::Interval::OneMinute       , ONE_MINUTE
    }
};

static const std::map<TradesDB::Interval, qint64> INTERVALS_VALUES = {
    {
        TradesDB::Interval::OneYear         , 365 * 24 * 60 * 60
    }, {
        TradesDB::Interval::SixMonths       , 188 * 24 * 60 * 60
    }, {
        TradesDB::Interval::OneMonth        , 30 * 24 * 60 * 60
    }, {
        TradesDB::Interval::OneWeek         , 7 * 24 * 60 * 60
    }, {
        TradesDB::Interval::TwentyFourHours , 24 * 60 * 60
    }, {
        TradesDB::Interval::TwelveHours     , 12 * 60 * 60
    }, {
        TradesDB::Interval::SixHours        , 5 * 60 * 60
    }, {
        TradesDB::Interval::OneHour         , 60 * 60
    }, {
        TradesDB::Interval::ThirtyMinutes   , 30 * 60
    }, {
        TradesDB::Interval::FifteenMinutes  , 15 * 60
    }, {
        TradesDB::Interval::OneMinute       , 60
    }
};


}

TradesDB::TradesDB(const std::shared_ptr<spdlog::logger> &logger
                   , const QString &dbFile
                   , QObject *parent)
    : QObject(parent)
    , logger_(logger)
    , requiredTables_({QStringLiteral("products"), QStringLiteral("trades"), QStringLiteral("data_points")})
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
        },

        {
            QStringLiteral("data_points"), [db = db_] {
                const QString query = QStringLiteral(
                "CREATE TABLE IF NOT EXISTS data_points ("
                "   rowid INTEGER PRIMARY KEY ASC,"
                "   productid INTEGER,"
                "   interval_label TEXT,"
                "   timestamp REAL,"
                "   open REAL,"
                "   high REAL,"
                "   low REAL,"
                "   close REAL,"
                "   volume REAL,"
                "   FOREIGN KEY(productid) REFERENCES products(rowid)"
                ");");
                if (!QSqlQuery(db).exec(query)) {
                    return false;
                }
                if (!QSqlQuery(db).exec(QStringLiteral("CREATE INDEX IF NOT EXISTS data_points_rowid ON data_points (rowid);"))) {
                    return false;
                }
                if (!QSqlQuery(db).exec(QStringLiteral("CREATE INDEX IF NOT EXISTS data_points_productid ON data_points (productid);"))) {
                    return false;
                }
                if (!QSqlQuery(db).exec(QStringLiteral("CREATE INDEX IF NOT EXISTS data_points_interval_label ON data_points (interval_label);"))) {
                    return false;
                }
                if (!QSqlQuery(db).exec(QStringLiteral("CREATE INDEX IF NOT EXISTS data_points_timestamp ON data_points (timestamp);"))) {
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
    for (const auto &pair : INTERVALS_LABELS) {
        const Interval &interval = pair.first;
        const QString &intervalLabel = pair.second;
        const QDateTime start = intervalStart(time, interval);
        query.prepare(QStringLiteral("SELECT * "
                                     "FROM data_points "
                                     "WHERE timestamp > :start AND "
                                     "  productid = :productid AND "
                                     "  interval_label = :interval_label; "));
        query.bindValue(QStringLiteral(":productid"), productid);
        query.bindValue(QStringLiteral(":start"), start.toMSecsSinceEpoch());
        query.bindValue(QStringLiteral(":interval_label"), intervalLabel);
        if (!query.exec()) {
            logger_->warn("[TradesDB] failed query {}: {}"
                          , query.lastQuery().toStdString()
                          , query.lastError().text().toStdString());
            db_.rollback();
            return false;
        }
        if (query.first()) {
            qint64 rowid = query.value(QStringLiteral("rowid")).toLongLong();
            qreal lastVolume = query.value(QStringLiteral("volume")).toReal();
            qreal high = query.value(QStringLiteral("high")).toReal();
            qreal low = query.value(QStringLiteral("low")).toReal();
            query.prepare(QStringLiteral("UPDATE data_points "
                                         "SET "
                                         "  timestamp = :timestamp, "
                                         "  high = :high, "
                                         "  low = :low, "
                                         "  close = :close, "
                                         "  volume = :volume "
                                         "WHERE rowid = :rowid; "));
            query.bindValue(QStringLiteral(":timestamp"), time.toMSecsSinceEpoch());
            query.bindValue(QStringLiteral(":rowid"), rowid);
            query.bindValue(QStringLiteral(":high"), price > high ? price : high);
            query.bindValue(QStringLiteral(":low"), price < low ? price : low);
            query.bindValue(QStringLiteral(":close"), price);
            query.bindValue(QStringLiteral(":volume"), lastVolume + volume);
        } else {
            query.prepare(QStringLiteral("INSERT INTO data_points ( "
                                         "  productid, "
                                         "  interval_label, "
                                         "  timestamp, "
                                         "  open, "
                                         "  high, "
                                         "  low, "
                                         "  close, "
                                         "  volume "
                                         ") "
                                         "VALUES ( "
                                         "  :productid, "
                                         "  :interval_label, "
                                         "  :timestamp, "
                                         "  :open, "
                                         "  :high, "
                                         "  :low, "
                                         "  :close, "
                                         "  :volume "
                                         "); "));
            query.bindValue(QStringLiteral(":productid"), productid);
            query.bindValue(QStringLiteral(":interval_label"), intervalLabel);
            query.bindValue(QStringLiteral(":timestamp"), time.toMSecsSinceEpoch());
            query.bindValue(QStringLiteral(":open"), price);
            query.bindValue(QStringLiteral(":high"), price);
            query.bindValue(QStringLiteral(":low"), price);
            query.bindValue(QStringLiteral(":close"), price);
            query.bindValue(QStringLiteral(":volume"), volume);
        }
        if (!query.exec()) {
            logger_->warn("[TradesDB] failed query {}: {}"
                          , query.lastQuery().toStdString()
                          , query.lastError().text().toStdString());
            db_.rollback();
            return false;
        }
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
    QDateTime start = sinceTime;
    QDateTime end = start.addSecs(stepDurationSecs);
    while (start < tillTime) {
        if (end > tillTime) {
            end = tillTime;
        }
        /*logger_->info("[TradesDB] get point for {} {} {}"
                      , product.toStdString()
                      , end.toString(Qt::ISODate).toStdString()
                      , stepDurationSecs);*/
        auto data = getDataPoint(product, end, stepDurationSecs);
        if (data) {
            result.push_back(data);
        }
        start = end;
        end = start.addSecs(stepDurationSecs);
    }
    return result;
}

const std::vector<TradesDB::DataPoint *> TradesDB::getDataPoints(
        const QString &product
        , TradesDB::Interval interval
        , qint64 maxCount)
{
    std::vector<TradesDB::DataPoint *> result;
    if (interval == Interval::Unknown) {
        interval = Interval::FifteenMinutes;
    }
//    QDateTime timeTill = QDateTime::currentDateTimeUtc();
    db_.transaction();
    QSqlQuery query(db_);
    query.prepare(QStringLiteral("SELECT rowid FROM products WHERE name = :name;"));
    query.bindValue(QStringLiteral(":name"), product);
    if (!query.exec() || !query.first()) {
        logger_->warn("[TradesDB] failed find product {}", product.toStdString());
        db_.rollback();
        return result;
    }
    qint64 productid = query.value(QStringLiteral("rowid")).toLongLong();
    const QString &intervalLabel = INTERVALS_LABELS.at(interval);
    query.prepare(QStringLiteral("SELECT * "
                                 "FROM data_points "
                                 "WHERE "
                                 "  productid = :productid AND "
                                 "  interval_label = :interval_label "
                                 "ORDER BY timestamp DESC "
                                 "LIMIT :max_count; "));
    query.bindValue(QStringLiteral(":productid"), productid);
    query.bindValue(QStringLiteral(":interval_label") , intervalLabel);
    query.bindValue(QStringLiteral(":max_count") , maxCount);

    if (!query.exec()) {
        logger_->warn("[TradesDB] failed read data: {} : {} using {}"
                      , query.lastError().text().toStdString()
                      , query.lastQuery().toStdString()
                      , productid);
        db_.rollback();
        return result;
    }
    if (!query.last()) {
        return result;
    }
    while (query.previous()) {
        qreal open = query.value(QStringLiteral("open")).toDouble();
        qreal high = query.value(QStringLiteral("high")).toDouble();
        qreal low = query.value(QStringLiteral("low")).toDouble();
        qreal close = query.value(QStringLiteral("close")).toDouble();
        qreal volume = query.value(QStringLiteral("volume")).toDouble();
        qreal timestamp = query.value(QStringLiteral("timestamp")).toDouble();
        QDateTime current = QDateTime::fromMSecsSinceEpoch(timestamp);
        auto point = new DataPoint { .open = open
                    , .high = high
                    , .low = low
                    , .close = close
                    , .volume = volume
                    , .timestamp = timestamp };
        if (result.size() > 0) {
            QDateTime expected = intervalStart(current, interval);
            qint64 step = qAbs(expected.msecsTo(current));
            QDateTime previous = QDateTime::fromMSecsSinceEpoch(result.back()->timestamp);
            while (previous.msecsTo(expected) > step) {
                auto emptyPoint = new DataPoint { .open = result.back()->close
                            , .high = result.back()->close
                            , .low = result.back()->close
                            , .close = result.back()->close
                            , .volume = 0
                            , .timestamp = timestamp + step };
                result.push_back(emptyPoint);
                previous = QDateTime::fromMSecsSinceEpoch(result.back()->timestamp);
            }
        }
        result.push_back(point);
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
    QDateTime time = QDateTime::currentDateTimeUtc().addYears(-1);
    QDateTime now = QDateTime::currentDateTimeUtc();
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

const QDateTime TradesDB::intervalStart(const QDateTime &now
                                        , const Interval &interval) const
{
    QDateTime result;
    switch (interval) {
    case TradesDB::Interval::OneYear:
        result = QDateTime(QDate(now.date().year(), 1, 1));
        break;
    case TradesDB::Interval::SixMonths: {
        int month = now.date().month();
        result = QDateTime(QDate(now.date().year(), month > 6 ? month - 6 : month, 1));
        break;
    }
    case TradesDB::Interval::OneMonth:
        result = QDateTime(QDate(now.date().year(), now.date().month(), 1));
        break;
    case TradesDB::Interval::OneWeek:
        result = QDateTime(now.date().addDays(-now.date().dayOfWeek()));
        break;
    case TradesDB::Interval::TwentyFourHours:
        result = QDateTime(now.date());
        break;
    case TradesDB::Interval::TwelveHours: {
        int hour = now.time().hour();
        hour = hour < 12 ? hour : hour - 12;
        result = QDateTime(now.date(), QTime(hour, 0));
        break;
    }
    case TradesDB::Interval::SixHours: {
        int hour = now.time().hour();
        int mod = hour % 6;
        hour = mod == 0 ? hour : hour - mod;
        result = QDateTime(now.date(), QTime(hour, 0));
        break;
    }
    case TradesDB::Interval::OneHour:
        result = QDateTime(now.date(), QTime(now.time().hour(), 0));
        break;
    case TradesDB::Interval::ThirtyMinutes: {
        int minute = now.time().minute();
        int mod = minute % 30;
        minute = mod == 0 ? minute : minute - mod;
        result = QDateTime(now.date(), QTime(now.time().hour(), minute));
        break;
    }
    case TradesDB::Interval::FifteenMinutes: {
        int minute = now.time().minute();
        int mod = minute % 15;
        minute = mod == 0 ? minute : minute - mod;
        result = QDateTime(now.date(), QTime(now.time().hour(), minute));
        break;
    }
    case TradesDB::Interval::OneMinute:
        break;
        result = QDateTime(now.date(), QTime(now.time().hour(), now.time().minute()));
    default:
        break;
    }
    return result;
}
