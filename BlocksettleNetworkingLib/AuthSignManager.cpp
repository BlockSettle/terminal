#include "AuthSignManager.h"

#include "AuthSignManager.h"
#include "ApplicationSettings.h"
#include "CelerClient.h"
#include "EncryptionUtils.h"
#include "AutheIDClient.h"

#include <spdlog/spdlog.h>


AuthSignManager::AuthSignManager(const std::shared_ptr<spdlog::logger> &logger
      , const std::shared_ptr<ApplicationSettings> &appSettings
      , const std::shared_ptr<CelerClient> &celerClient
      , const std::shared_ptr<ConnectionManager> &connectionManager)
   : logger_(logger)
   , appSettings_(appSettings)
   , celerClient_(celerClient)
   , autheIDClient_(new AutheIDClient(logger, appSettings, connectionManager))
{
   connect(autheIDClient_.get(), &AutheIDClient::signSuccess, this, &AuthSignManager::onSignSuccess);
   connect(autheIDClient_.get(), &AutheIDClient::failed, this, &AuthSignManager::onFailed);
}

AuthSignManager::~AuthSignManager() noexcept = default;

bool AuthSignManager::Sign(const BinaryData &dataToSign, const QString &title, const QString &desc
   , const SignedCb &onSigned, const SignFailedCb &onSignFailed, int expiration)
{
   onSignedCB_ = onSigned;
   onSignFailedCB_ = onSignFailed;
   const auto &userId = celerClient_->userName();
   logger_->debug("[{}] sending sign {} request to {}", __func__, title.toStdString(), userId);
   autheIDClient_->sign(dataToSign, userId, title, desc, expiration);
   return true;
}

void AuthSignManager::onFailed(QNetworkReply::NetworkError error, AutheIDClient::ErrorType authError)
{
   logger_->error("[AuthSignManager] Auth eID failure: {}", AutheIDClient::errorString(authError).toStdString());
   if (onSignFailedCB_) {
      onSignFailedCB_(AutheIDClient::errorString(authError));
   }
   onSignedCB_ = {};
   onSignFailedCB_ = {};
}

void AuthSignManager::onSignSuccess(const std::string &data, const BinaryData &invisibleData
   , const std::string &signature)
{
   logger_->debug("[AuthSignManager] data signed");
   if (onSignedCB_) {
      onSignedCB_(data, invisibleData, signature);
   }
   onSignedCB_ = {};
   onSignFailedCB_ = {};
}
