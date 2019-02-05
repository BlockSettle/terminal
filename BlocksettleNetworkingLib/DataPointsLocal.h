#ifndef DATAPOINTSLOCAL_H
#define DATAPOINTSLOCAL_H

#include <QObject>
#include <QtSql/QSqlDatabase>

#include <spdlog/spdlog.h>

class DataPointsLocal : public QObject
{
    Q_OBJECT
public:
    enum Interval {
        Unknown = -1,
        OneYear,
        SixMonths,
        OneMonth,
        OneWeek,
        TwentyFourHours,
        TwelveHours,
        SixHours,
        OneHour
    };
    struct DataPoint {
        qreal open = -1.0;
        qreal high = -1.0;
        qreal low = -1.0;
        qreal close = -1.0;
        qreal volume = -1.0;
        qreal timestamp = -1.0;
        bool isValid() {
            return timestamp != -1.0;
        }
    };
    explicit DataPointsLocal(
            const std::string &databaseHost
            , const std::string &databasePort
            , const std::string &databaseName
            , const std::string &databaseUser
            , const std::string &databasePassword
            , const std::shared_ptr<spdlog::logger> &logger
            , QObject *parent = nullptr);

    const uint64_t getLatestTimestamp(const std::string &product
                                      , const Interval &interval = OneHour) const;

    const std::vector<DataPoint *> getDataPoints(const std::string &product
                                                 , Interval interval = Interval::Unknown
                                                 , qint64 maxCount = 100);

private:
   static const QStringList getAllTableNames();
   static const std::string getTable(const std::string &product, Interval interval);
   const uint64_t intervalStart(const uint64_t &timestamp, const Interval &interval) const;
   const uint64_t intervalEnd(const uint64_t &timestamp, const Interval &interval) const;

private:
   std::shared_ptr<spdlog::logger>     logger_;
   QSqlDatabase                        db_;
   const QStringList                   requiredTables_;

   using createTableFunc = std::function<bool(void)>;
   std::map<QString, createTableFunc>  createTable_;
};

#endif // DATAPOINTSLOCAL_H
