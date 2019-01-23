#ifndef CHARTSCLIENT_H
#define CHARTSCLIENT_H

#include <memory>

#include <QObject>

namespace spdlog {
   class logger;
}

class ApplicationSettings;
class TradesDB;


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

private:
    std::shared_ptr<ApplicationSettings>   appSettings_;
    std::shared_ptr<spdlog::logger>        logger_;

    std::unique_ptr<TradesDB>              tradesDb_;
};

#endif // CHARTSCLIENT_H
