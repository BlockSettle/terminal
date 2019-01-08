#include "AuthSignManager.h"

#include "AuthSignManager.h"
#include "ApplicationSettings.h"
#include "CelerClient.h"
#include "EncryptionUtils.h"
#include "AutheIDClient.h"

#include <spdlog/spdlog.h>


AuthSignManager::AuthSignManager(const std::shared_ptr<spdlog::logger> &logger
      , const std::shared_ptr<ApplicationSettings> &appSettings
      , const std::shared_ptr<CelerClient> &celerClient)
   : logger_(logger)
   , appSettings_(appSettings)
   , celerClient_(celerClient)
   , autheIDClient_(new AutheIDClient(logger, appSettings_->GetAuthKeys()))
{
   connect(autheIDClient_.get(), &AutheIDClient::signSuccess, this, &AuthSignManager::onSignSuccess);
   connect(autheIDClient_.get(), &AutheIDClient::failed, this, &AuthSignManager::onFailed);
}

AuthSignManager::~AuthSignManager() = default;

bool AuthSignManager::Sign(const BinaryData &dataToSign, const QString &title, const QString &desc
   , const SignedCb &onSigned, const SignFailedCb &onSignFailed, int expiration)
{
   callbacks_ = { onSigned, onSignFailed };
   try {
      autheIDClient_->connect(appSettings_->get<std::string>(ApplicationSettings::authServerPubKey)
         , appSettings_->get<std::string>(ApplicationSettings::authServerHost)
         , appSettings_->get<std::string>(ApplicationSettings::authServerPort));
   }
   catch (const std::exception &e) {
      logger_->error("[{}] failed to connect: {}", __func__, e.what());
      if (onSignFailed) {
         onSignFailed(tr("Failed to connect to Auth eID"));
      }
      return false;
   }
   const auto &userId = celerClient_->userName();
   logger_->debug("[{}] sending sign {} request to {}", __func__, title.toStdString(), userId);
   if (!autheIDClient_->sign(dataToSign, userId, title, desc, expiration)) {
      if (onSignFailed) {
         onSignFailed(tr("Failed to sign with Auth eID"));
      }
      return false;
   }
   return true;
}

void AuthSignManager::onFailed(const QString &text)
{
   logger_->error("[AuthSignManager] Auth eID failure: {}", text.toStdString());
   if (callbacks_.second) {
      callbacks_.second(text);
   }
   callbacks_ = { nullptr, nullptr };
}

void AuthSignManager::onSignSuccess(const std::string &data, const BinaryData &invisibleData
   , const std::string &signature)
{
   logger_->debug("[AuthSignManager] data signed");
   if (callbacks_.first) {
      callbacks_.first(data, invisibleData, signature);
   }
   callbacks_ = { nullptr, nullptr };
}
