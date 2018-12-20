#ifndef __AUTH_EID_CLIENT_H__
#define __AUTH_EID_CLIENT_H__

#include <QObject>
#include <QTimer>

#include "DataConnectionListener.h"
#include "ZmqSecuredDataConnection.h"
#include "EncryptUtils.h"

#include "rp_api.pb.h"


namespace spdlog {
   class logger;
}

class ConnectionManager;
class ApplicationSettings;
class MobileClient;


class AutheIDClient : public QObject
{
    Q_OBJECT

public:
    AutheIDClient(const std::shared_ptr<spdlog::logger>& logger
                  , const std::shared_ptr<ApplicationSettings>& appSettings);
    ~AutheIDClient() noexcept override;

    bool authenticate(const std::string email);

signals:
    void authDone(const std::string email);
    void authFailed();

private slots:
    void onAuthSuccess(const std::string &jwt);
    void onFailed(const QString &);

private:
    std::shared_ptr<spdlog::logger>        logger_;
    std::shared_ptr<ApplicationSettings>   appSettings_;
    std::unique_ptr<MobileClient>          mobileClient_;

    std::string requestId_;
    std::string email_;
};

#endif
