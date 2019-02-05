#ifndef CHARTSCLIENT_H
#define CHARTSCLIENT_H

#include <memory>

#include <QObject>

#include "CommonTypes.h"
//#include "TradesDB.h"
#include "DataPointsLocal.h"

namespace spdlog {
   class logger;
}

class ApplicationSettings;


class TradesClient : public QObject
{
    Q_OBJECT
public:
    TradesClient(const std::shared_ptr<ApplicationSettings> &appSettings
                 , const std::shared_ptr<spdlog::logger>& logger
                 , QObject *parent = nullptr);
    ~TradesClient() noexcept override;

    TradesClient(const TradesClient&) = delete;
    TradesClient& operator = (const TradesClient&) = delete;
    TradesClient(TradesClient&&) = delete;
    TradesClient& operator = (TradesClient&&) = delete;

    void init();

    const std::vector<DataPointsLocal::DataPoint*> getRawPointDataArray(
            const QString &product
            , DataPointsLocal::Interval interval = DataPointsLocal::Interval::Unknown
            , qint64 maxCount = 100);

public slots:
    void onMDUpdated(bs::network::Asset::Type assetType
                     , const QString &security
                     , bs::network::MDFields fields);

private:
    std::shared_ptr<ApplicationSettings>   appSettings_;
    std::shared_ptr<spdlog::logger>        logger_;

    std::unique_ptr<DataPointsLocal>       tradesDb_;
};

#endif // CHARTSCLIENT_H
