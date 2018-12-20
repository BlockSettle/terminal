#include "AutheIDClient.h"

#include "ApplicationSettings.h"
#include "ConnectionManager.h"
#include "ZmqSecuredDataConnection.h"
#include "MobileClient.h"

#include "EncryptUtils.h"

#include <spdlog/spdlog.h>
#include "botan/base64.h"


AutheIDClient::AutheIDClient(const std::shared_ptr<spdlog::logger>& logger
                             , const std::shared_ptr<ApplicationSettings>& appSettings)
     : logger_(logger)
     , appSettings_(appSettings)
     , mobileClient_(new MobileClient(logger, appSettings_->GetAuthKeys()))
{
    connect(mobileClient_.get(), &MobileClient::authSuccess, this, &AutheIDClient::onAuthSuccess);
    connect(mobileClient_.get(), &MobileClient::failed, this, &AutheIDClient::onFailed);
}


AutheIDClient::~AutheIDClient() noexcept = default;


bool AutheIDClient::authenticate(const std::string email)
{
    email_ = email;
    try {
       mobileClient_->connect(appSettings_->get<std::string>(ApplicationSettings::authServerPubKey)
          , appSettings_->get<std::string>(ApplicationSettings::authServerHost)
          , appSettings_->get<std::string>(ApplicationSettings::authServerPort));
    }
    catch (const std::exception &e) {
       logger_->error("[{}] failed to connect: {}", __func__, e.what());
       return false;
    }

    logger_->debug("Requested authentication for {0} ...", email_);

    if (!mobileClient_->authenticate(email))
    {
        logger_->error("Authentication failed for {0}", email_);
        return false;
    }

    return true;
}


void AutheIDClient::onFailed(const QString &text)
{
    logger_->error("Authentication failed for {0}: {1}", email_, text.toStdString());

    emit authFailed();
}


void AutheIDClient::onAuthSuccess(const std::string &jwt)
{
    logger_->debug("Authentication passed for {0}(JWT): {1}", email_, jwt);

    emit authDone(email_);
}
