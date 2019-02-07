#include "DataPointsLocal.h"

#include <QtCore/QDateTime>
#include <QtCore/QVariant>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>

// Private Market
const std::string ANT_XBT = "ANT/XBT";
const std::string BLK_XBT = "BLK/XBT";
const std::string BSP_XBT = "BSP/XBT";
const std::string JAN_XBT = "JAN/XBT";
const std::string SCO_XBT = "SCO/XBT";
// Spot XBT
const std::string XBT_EUR = "XBT/EUR";
const std::string XBT_GBP = "XBT/GBP";
const std::string XBT_JPY = "XBT/JPY";
const std::string XBT_SEK = "XBT/SEK";
// Spot FX
const std::string EUR_GBP = "EUR/GBP";
const std::string EUR_JPY = "EUR/JPY";
const std::string EUR_SEK = "EUR/SEK";
const std::string GPB_JPY = "GPB/JPY";
const std::string GBP_SEK = "GBP/SEK";
const std::string JPY_SEK = "JPY/SEK";

static const std::map<std::string, std::string> PRODUCT_MAPPER = {
   // Private Market
   /*{ ANT_XBT, "ant_xbt" },
   { BLK_XBT, "blk_xbt" },
   { BSP_XBT, "bsp_xbt" },
   { JAN_XBT, "jan_xbt" },
   { SCO_XBT, "sco_xbt" },*/
   // Spot XBT
   /*{ XBT_EUR, "xbt_eur" },
   { XBT_GBP, "xbt_gbp" },
   { XBT_JPY, "xbt_jpy" },
   { XBT_SEK, "xbt_sek" },*/
   // Spot FX
   { EUR_GBP, "eur_gbp" },
   { EUR_JPY, "eur_jpy" },
   { EUR_SEK, "eur_sek" },
   { GPB_JPY, "gpb_jpy" },
   { GBP_SEK, "gbp_sek" },
   { JPY_SEK, "jpy_sek" }
};

static const std::map<DataPointsLocal::Interval, std::string> INTERVAL_MAPPER = {
   { DataPointsLocal::Interval::OneYear         , "1y"   },
   { DataPointsLocal::Interval::SixMonths       , "6m"   },
   { DataPointsLocal::Interval::OneMonth        , "1m"   },
   { DataPointsLocal::Interval::OneWeek         , "1w"   },
   { DataPointsLocal::Interval::TwentyFourHours , "24h"  },
   { DataPointsLocal::Interval::TwelveHours     , "12h"  },
   { DataPointsLocal::Interval::SixHours        , "6h"   },
   { DataPointsLocal::Interval::OneHour         , "1h"   }
};

DataPointsLocal::DataPointsLocal(const std::string &databaseHost
                                 , const std::string &databasePort
                                 , const std::string &databaseName
                                 , const std::string &databaseUser
                                 , const std::string &databasePassword
                                 , const std::shared_ptr<spdlog::logger> &logger
                                 , QObject *parent)
    : QObject(parent)
    , logger_(logger)
    , requiredTables_(DataPointsLocal::getAllTableNames())
{
    db_ = QSqlDatabase::addDatabase(QStringLiteral("QMYSQL"), QStringLiteral("mdhs"));
    db_.setHostName(QString::fromStdString(databaseHost));
    db_.setPort(QString::fromStdString(databasePort).toInt());
    db_.setDatabaseName(QString::fromStdString(databaseName));
    db_.setUserName(QString::fromStdString(databaseUser));
    db_.setPassword(QString::fromStdString(databasePassword));

    if (!db_.open()) {
       throw std::runtime_error("failed to open " + db_.connectionName().toStdString()
                                + " DB: " + db_.lastError().text().toStdString());
    }
}

const uint64_t DataPointsLocal::getLatestTimestamp(const std::string &product
                                                   , const DataPointsLocal::Interval &interval) const
{
    QString table = QString::fromStdString(getTable(product, interval));
    if (table.isEmpty()) {
       throw std::logic_error("empty");
    }
    QSqlQuery query(db_);
    if (!query.exec(QStringLiteral("SELECT timestamp "
                                   "FROM %1 "
                                   "ORDER BY timestamp DESC "
                                   "LIMIT 1; ").arg(table))) {
       logger_->warn("[DataPointsDB] failed query latest timestap for {} within {} : {}"
                     , product
                     , INTERVAL_MAPPER.at(interval)
                     , query.lastError().text().toStdString());
       throw std::logic_error("error");
    }
    if (!query.first()) {
       return 0;
    }
    return query.value(QStringLiteral("timestamp")).toULongLong();
}

const std::vector<DataPointsLocal::DataPoint *> DataPointsLocal::getDataPoints(const std::string &product
        , DataPointsLocal::Interval interval
        , qint64 maxCount)
{
    std::vector<DataPointsLocal::DataPoint *> result;
    if (interval == Interval::Unknown) {
        interval = Interval::OneHour;
    }
    db_.transaction();
    QSqlQuery query(db_);
    QString table = QString::fromStdString(getTable(product, interval));

    query.prepare(QStringLiteral("SELECT * "
                                 "FROM %1 "
                                 "ORDER BY timestamp DESC "
                                 "LIMIT :max_count; ").arg(table));
    query.bindValue(QStringLiteral(":max_count") , maxCount);
    if (!query.exec()) {
        logger_->warn("[TradesDB] failed read data: {} : {}"
                      , query.lastError().text().toStdString()
                      , query.lastQuery().toStdString());
        db_.rollback();
        return result;
    }
    if (!query.last()) {
        return result;
    }
    query.next();
    while (query.previous()) {
        qreal open = query.value(QStringLiteral("open")).toDouble();
        qreal high = query.value(QStringLiteral("high")).toDouble();
        qreal low = query.value(QStringLiteral("low")).toDouble();
        qreal close = query.value(QStringLiteral("close")).toDouble();
        qreal volume = query.value(QStringLiteral("volume")).toDouble();
        qint64 timestamp = query.value(QStringLiteral("timestamp")).toLongLong();
        auto point = createDataPoint(open, high, low, close, volume, timestamp);
        result.push_back(point);
    }
    if (result.size() > 0) {
       auto lastPoint = result.back();
       qint64 lastTimestamp = lastPoint->timestamp;
       qint64 expectedLastTimestamp = intervalEnd(lastTimestamp, interval);
       if (lastTimestamp < expectedLastTimestamp) {
          lastPoint->timestamp = expectedLastTimestamp;
       }

       auto now = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
       auto step = intervalEnd(now, interval) - intervalStart(now, interval);
       auto next = now + step;
       while (now > next) {
          auto emptyPoint = createDataPoint(result.back()->close
                                            , result.back()->close
                                            , result.back()->close
                                            , result.back()->close
                                            , 0
                                            , next);
          result.push_back(emptyPoint);
          next += step;
       }
    }
    return result;
}

const QStringList DataPointsLocal::getAllTableNames()
{
    QStringList result;
    for (const auto& i : PRODUCT_MAPPER) {
       const std::string& product = i.first;
       for (const auto& j : INTERVAL_MAPPER) {
          const Interval& interval = j.first;
          result.append(QString::fromStdString(getTable(product, interval)));
       }
    }
    return result;
}

const std::string DataPointsLocal::getTable(const std::string &product
                                            , DataPointsLocal::Interval interval)
{
    auto productIterator = PRODUCT_MAPPER.find(product);
    std::string productLabel = productIterator != PRODUCT_MAPPER.end()
          ? productIterator->second : "";
    auto intervalIterator = INTERVAL_MAPPER.find(interval);
    std::string intervalLabel = intervalIterator != INTERVAL_MAPPER.end()
          ? intervalIterator->second : "";
    if (productLabel.empty() || intervalLabel.empty()) {
       return "";
    }
    return "data_points_" + productLabel + "_" + intervalLabel;
}

const uint64_t DataPointsLocal::intervalStart(const uint64_t &timestamp
                                              , const DataPointsLocal::Interval &interval) const
{
    QDateTime now = QDateTime::fromMSecsSinceEpoch(timestamp).toUTC();
    QDateTime result = now;
    switch (interval) {
    case DataPointsLocal::Interval::OneYear: {
       result.setTime(QTime(0, 0));
       result.setDate(QDate(now.date().year(), 1, 1));
       break;
    }
    case DataPointsLocal::Interval::SixMonths: {
       int month = now.date().month(); // 1 - January, 12 - December
       int mod = month % 6;
       result.setTime(QTime(0, 0));
       result.setDate(QDate(now.date().year(), month - mod + 1, 1));
       break;
    }
    case DataPointsLocal::Interval::OneMonth: {
       result.setTime(QTime(0, 0));
       result.setDate(QDate(now.date().year(), now.date().month(), 1));
       break;
    }
    case DataPointsLocal::Interval::OneWeek: {
       auto date = now.date();
       auto start = date.addDays(1 - date.dayOfWeek()); //1 - Monday, 7 - Sunday
       result.setTime(QTime(0, 0));
       result.setDate(start);
       break;
    }
    case DataPointsLocal::Interval::TwentyFourHours:
       result.setTime(QTime(0, 0));
       break;
    case DataPointsLocal::Interval::TwelveHours: {
       int hour = now.time().hour();
       int mod = hour % 12;
       result.setTime(QTime(hour - mod, 0));
       break;
    }
    case DataPointsLocal::Interval::SixHours: {
       int hour = now.time().hour();
       int mod = hour % 6;
       result.setTime(QTime(hour - mod, 0));
       break;
    }
    case DataPointsLocal::Interval::OneHour:
       result.setTime(QTime(now.time().hour(), 0));
       break;
    default:
       break;
    }
    return result.toMSecsSinceEpoch();
}

const uint64_t DataPointsLocal::intervalEnd(const uint64_t &timestamp
                                            , const DataPointsLocal::Interval &interval) const
{
    auto start = QDateTime::fromMSecsSinceEpoch(intervalStart(timestamp, interval)).toUTC();
    QDateTime result = start;
    switch (interval) {
    case DataPointsLocal::Interval::OneYear: {
       result = result.addYears(1);
        break;
    }
    case DataPointsLocal::Interval::SixMonths: {
       result = result.addMonths(6);
        break;
    }
    case DataPointsLocal::Interval::OneMonth: {
       result = result.addMonths(1);
        break;
    }
    case DataPointsLocal::Interval::OneWeek: {
       result = result.addDays(7);
        break;
    }
    case DataPointsLocal::Interval::TwentyFourHours:
       result = result.addDays(1);
        break;
    case DataPointsLocal::Interval::TwelveHours: {
       result = result.addSecs(12 * 3600);
        break;
    }
    case DataPointsLocal::Interval::SixHours: {
       result = result.addSecs(6 * 3600);
        break;
    }
    case DataPointsLocal::Interval::OneHour:
       result = result.addSecs(3600);
        break;
    default:
        break;
    }
    return result.toMSecsSinceEpoch();
}

DataPointsLocal::DataPoint *DataPointsLocal::createDataPoint(double open
                                                             , double high
                                                             , double low
                                                             , double close
                                                             , double volume
                                                             , const uint64_t &timestamp) const
{
   return new DataPoint { .open = open
            , .high = high
            , .low = low
            , .close = close
            , .volume = volume
            , .timestamp = static_cast<qreal>(timestamp) };
}
