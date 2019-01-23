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
    TradesDB(const std::shared_ptr<spdlog::logger> &logger,
             const QString &dbFile,
             QObject *parent = nullptr);
    ~TradesDB() noexcept = default;

    TradesDB(const TradesDB&) = delete;
    TradesDB &operator=(const TradesDB&) = delete;
    TradesDB(TradesDB&&) = delete;
    TradesDB& operator=(TradesDB&&) = delete;

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
