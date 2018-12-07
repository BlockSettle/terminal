#include "AuthSignManager.h"

#include "CelerClient.h"
#include "ApplicationSettings.h"
#include "EncryptionUtils.h"

#include <cassert>
#include <spdlog/spdlog.h>


AuthSignManager::AuthSignManager(const std::shared_ptr<spdlog::logger> &logger
      , const std::shared_ptr<ApplicationSettings> &appSettings)
 : logger_(logger)
 , applicationSettings_(appSettings)
{
}

bool AuthSignManager::Sign(const SecureBinaryData &dataToSign, const SignedCb &onSigned)
{
   onSigned(std::string("temporary fake signature"));
   return true;
}
