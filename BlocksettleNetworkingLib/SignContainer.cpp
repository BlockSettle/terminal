#include "SignContainer.h"

#include "ApplicationSettings.h"
#include "ConnectionManager.h"
#include "HDWallet.h"
#include "HeadlessContainer.h"
#include "OfflineSigner.h"

#include <spdlog/spdlog.h>

Q_DECLARE_METATYPE(std::shared_ptr<bs::hd::Wallet>)


SignContainer::SignContainer(const std::shared_ptr<spdlog::logger> &logger, OpMode opMode)
   : logger_(logger), mode_(opMode)
{
   qRegisterMetaType<std::shared_ptr<bs::hd::Wallet>>();
}


std::shared_ptr<SignContainer> CreateSigner(const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<ApplicationSettings> &appSettings
   , const std::shared_ptr<ConnectionManager>& connectionManager)
{
   const auto &port = appSettings->get<QString>(ApplicationSettings::signerPort);
   const auto &pwHash = appSettings->get<QString>(ApplicationSettings::signerPassword);
   const auto runMode = appSettings->get<int>(ApplicationSettings::signerRunMode);
   switch (static_cast<SignContainer::OpMode>(runMode))
   {
   case SignContainer::OpMode::Local:
      return std::make_shared<LocalSigner>(logger, appSettings->GetHomeDir()
         , appSettings->get<NetworkType>(ApplicationSettings::netType), port
         , connectionManager, pwHash
         , appSettings->get<double>(ApplicationSettings::autoSignSpendLimit));

   case SignContainer::OpMode::Remote:
      return std::make_shared<RemoteSigner>(logger
         , appSettings->get<QString>(ApplicationSettings::signerHost), port, connectionManager, pwHash);

   case SignContainer::OpMode::Offline:
      return std::make_shared<OfflineSigner>(logger, appSettings->get<QString>(ApplicationSettings::signerOfflineDir));

   default:
      logger->error("[CreateSigner] Unknown signer run mode");
      break;
   }
   return nullptr;
}
