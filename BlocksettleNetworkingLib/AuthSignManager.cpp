#include "AuthSignManager.h"

#include "ApplicationSettings.h"
#include "AutheIDClient.h"
#include "AuthSignManager.h"
#include "CelerClient.h"
#include "ConnectionManager.h"
#include "EncryptionUtils.h"

#include <spdlog/spdlog.h>


AuthSignManager::AuthSignManager(const std::shared_ptr<spdlog::logger> &logger
      , const std::shared_ptr<ApplicationSettings> &appSettings
      , const std::shared_ptr<BaseCelerClient> &celerClient
      , const std::shared_ptr<ConnectionManager> &connectionManager)
   : logger_(logger)
   , appSettings_(appSettings)
   , celerClient_(celerClient)
   , connectionManager_(connectionManager)
{
}

AuthSignManager::~AuthSignManager() noexcept = default;

bool AuthSignManager::Sign(const BinaryData &dataToSign, const QString &title, const QString &desc
   , const SignedCb &onSigned, const SignFailedCb &onSignFailed, int expiration)
{
   // recreate autheIDClient in case there another request in flight (it should be stopped)
   autheIDClient_.reset(new AutheIDClient(logger_, connectionManager_->GetNAM(), appSettings_->GetAuthKeys(), appSettings_->isAutheidTestEnv()));
   connect(autheIDClient_.get(), &AutheIDClient::signSuccess, this, &AuthSignManager::onSignSuccess);
   connect(autheIDClient_.get(), &AutheIDClient::failed, this, &AuthSignManager::onFailed);

   onSignedCB_ = onSigned;
   onSignFailedCB_ = onSignFailed;
   const auto &userId = celerClient_->userName();
   logger_->debug("[{}] sending sign {} request to {}", __func__, title.toStdString(), userId);
   AutheIDClient::SignRequest request;
   request.email = userId;
   request.title = title.toStdString();
   request.description = desc.toStdString();
   request.expiration = expiration;
   request.invisibleData = dataToSign;
   autheIDClient_->sign(request);
   return true;
}

void AuthSignManager::onFailed(AutheIDClient::ErrorType authError)
{
   logger_->error("[AuthSignManager] Auth eID failure: {}", AutheIDClient::errorString(authError).toStdString());
   if (onSignFailedCB_) {
      onSignFailedCB_(AutheIDClient::errorString(authError));
   }
   onSignedCB_ = {};
   onSignFailedCB_ = {};
}

void AuthSignManager::onSignSuccess(const AutheIDClient::SignResult &result)
{
   logger_->debug("[AuthSignManager] data signed");
   if (onSignedCB_) {
      onSignedCB_(result);
   }
   onSignedCB_ = {};
   onSignFailedCB_ = {};
}
