#include "SignContainer.h"

#include "ApplicationSettings.h"
#include "ConnectionManager.h"
#include "HeadlessContainer.h"
#include "OfflineSigner.h"

#include <QTcpSocket>
#include <spdlog/spdlog.h>

Q_DECLARE_METATYPE(std::shared_ptr<bs::hd::Wallet>)


SignContainer::SignContainer(const std::shared_ptr<spdlog::logger> &logger, OpMode opMode)
   : logger_(logger), mode_(opMode)
{
   qRegisterMetaType<std::shared_ptr<bs::hd::Wallet>>();
}

// Create the signer object (local, remote, or offline). Note that the callbacks
// are null by default and aren't required by the offline signer.
//
// INPUT:  The logger to use. (const std::shared_ptr<spdlog::logger>)
//         The application settings. (const std::shared_ptr<ApplicationSettings>)
//         The signer mode (local/remote/offline). (SignContainer::OpMode)
//         The host address. (const QString)
//         The signer connection manager. (const std::shared_ptr<ConnectionManager>)
//         A flag indicating if data conn will use ephemeral ID keys. (const bool)
//         The callback to invoke on a new BIP 150 ID key. (const std::function)
// OUTPUT: N/A
// RETURN: A pointer to the signer object.
std::shared_ptr<SignContainer> CreateSigner(const std::shared_ptr<spdlog::logger> &logger
   , const std::shared_ptr<ApplicationSettings> &appSettings
   , SignContainer::OpMode runMode, const QString &host
   , const std::shared_ptr<ConnectionManager>& connectionManager
   , const bool& ephemeralDataConnKeys
   , const ZmqBIP15XDataConnection::cbNewKey& inNewKeyCB)
{
   if (connectionManager == nullptr) {
      logger->error("[{}] need connection manager to create signer", __func__);
      return nullptr;
   }

   const auto &port = appSettings->get<QString>(ApplicationSettings::signerPort);
   const auto netType = appSettings->get<NetworkType>(ApplicationSettings::netType);

   switch (runMode)
   {
   case SignContainer::OpMode::Local:
      return std::make_shared<LocalSigner>(logger, appSettings->GetHomeDir()
         , netType, port, connectionManager, appSettings, runMode
         , appSettings->get<double>(ApplicationSettings::autoSignSpendLimit)
         , ephemeralDataConnKeys, inNewKeyCB);

   case SignContainer::OpMode::Remote:
      return std::make_shared<RemoteSigner>(logger, host, port, netType
         , connectionManager, appSettings, runMode, ephemeralDataConnKeys
         , inNewKeyCB);

   default:
      logger->error("[{}] Unknown signer run mode {}", __func__, (int)runMode);
      break;
   }
   return nullptr;
}

bool SignerConnectionExists(const QString &host, const QString &port)
{
   QTcpSocket sock;
   sock.connectToHost(host, port.toUInt());
   return sock.waitForConnected(30);
}
