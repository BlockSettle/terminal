#ifndef __TRADESDB_H__
#define __TRADESDB_H__

#include <map>
#include <functional>
#include <memory>

#include <QObject>
#include <QtSql/QSqlDatabase>

namespace spdlog {
   class logger;
}

class TradesDB : public QObject
{
    Q_OBJECT
public:
    struct DataPoint {
        qreal open = 0.0;
        qreal high = 0.0;
        qreal low = 0.0;
        qreal close = 0.0;
        qreal volume = 0.0;
        qreal timestamp = 0.0;
        bool isValid() {
            return timestamp != 0.0;
        }
    };
    TradesDB(const std::shared_ptr<spdlog::logger> &logger,
             const QString &dbFile,
             QObject *parent = nullptr);
    ~TradesDB() noexcept = default;

    TradesDB(const TradesDB&) = delete;
    TradesDB &operator=(const TradesDB&) = delete;
    TradesDB(TradesDB&&) = delete;
    TradesDB& operator=(TradesDB&&) = delete;

    DataPoint getDataPoint(const QString &product
                           , const QDateTime &timeTill
                           , qint64 durationSec);

    bool add(const QString &product
                  , const QDateTime &time
                  , const qreal &price
                  , const qreal &volume);

private:
    bool createMissingTables();
    bool populateEmptyData();

private:
    std::shared_ptr<spdlog::logger>     logger_;
    QSqlDatabase                        db_;
    const QStringList                   requiredTables_;

    using createTableFunc = std::function<bool(void)>;
    std::map<QString, createTableFunc>  createTable_;
};

#endif // __TRADESDB_H__
