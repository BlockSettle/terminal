#include "ArmoryServersProvider.h"
#include "ArmoryConnection.h"

#include <QDir>
#include <QStandardPaths>

namespace {

   // Change to true if local ArmoryDB auto-start should be reverted (not tested, see BST-2131)
   const bool kEnableLocalAutostart = false;

} // namespace

const QList<ArmoryServer> ArmoryServersProvider::defaultServers_ = {
   ArmoryServer::fromTextSettings(QStringLiteral(ARMORY_BLOCKSETTLE_NAME":0:armory.blocksettle.com:80:")),
   ArmoryServer::fromTextSettings(QStringLiteral(ARMORY_BLOCKSETTLE_NAME":1:armory.blocksettle.com:81:")),
   ArmoryServer::fromTextSettings(kEnableLocalAutostart ?
      QStringLiteral("%1:0:127.0.0.1::").arg(tr("Local Auto-launch Node")) :
      QStringLiteral("%1:0:127.0.0.1::").arg(tr("Local ArmoryDB Node"))),
   ArmoryServer::fromTextSettings(kEnableLocalAutostart ?
      QStringLiteral("%1:1:127.0.0.1::").arg(tr("Local Auto-launch Node")) :
      QStringLiteral("%1:1:127.0.0.1:81:").arg(tr("Local ArmoryDB Node")))
};

const int ArmoryServersProvider::kDefaultServersCount = ArmoryServersProvider::defaultServers_.size();

ArmoryServersProvider::ArmoryServersProvider(const std::shared_ptr<ApplicationSettings> &appSettings, QObject *parent)
   : QObject(parent)
   , appSettings_(appSettings)
{
}

QList<ArmoryServer> ArmoryServersProvider::servers() const
{
   QStringList userServers = appSettings_->get<QStringList>(ApplicationSettings::armoryServers);

   QList<ArmoryServer> servers;
   QStringList defaultServersKeys = appSettings_->get<QStringList>(ApplicationSettings::defaultArmoryServersKeys);

   // #1 add MainNet blocksettle server
   ArmoryServer bsMainNet = defaultServers_.at(0);
   if (defaultServersKeys.size() >= 1) {
      bsMainNet.armoryDBKey = defaultServersKeys.at(0);
   }
   servers.append(bsMainNet);

   // #2 add TestNet blocksettle server
   ArmoryServer bsTestNet = defaultServers_.at(1);
   if (defaultServersKeys.size() >= 2) {
      bsTestNet.armoryDBKey = defaultServersKeys.at(1);
   }
   servers.append(bsTestNet);

   // #3 add localhost node MainNet
   ArmoryServer localMainNet = defaultServers_.at(2);
   localMainNet.armoryDBPort = appSettings_->GetDefaultArmoryLocalPort(NetworkType::MainNet);
   localMainNet.runLocally = kEnableLocalAutostart;
   if (defaultServersKeys.size() >= 3) {
      localMainNet.armoryDBKey = defaultServersKeys.at(2);
   }
   servers.append(localMainNet);

   // #4 add localhost node TestNet
   ArmoryServer localTestNet = defaultServers_.at(3);
   localTestNet.armoryDBPort = appSettings_->GetDefaultArmoryLocalPort(NetworkType::TestNet);
   localTestNet.runLocally = kEnableLocalAutostart;
   if (defaultServersKeys.size() >= 4) {
      localTestNet.armoryDBKey = defaultServersKeys.at(3);
   }
   servers.append(localTestNet);

   for (const QString &srv : userServers) {
      servers.append(ArmoryServer::fromTextSettings(srv));
   }

   return servers;
}

ArmorySettings ArmoryServersProvider::getArmorySettings() const
{
   ArmorySettings settings;

   settings.netType = appSettings_->get<NetworkType>(ApplicationSettings::netType);
   settings.armoryDBIp = appSettings_->get<QString>(ApplicationSettings::armoryDbIp);
   settings.armoryDBPort = appSettings_->GetArmoryRemotePort();
   settings.runLocally = appSettings_->get<bool>(ApplicationSettings::runArmoryLocally);

   const int serverIndex = indexOf(static_cast<ArmoryServer>(settings));
   if (serverIndex >= 0) {
      settings.armoryDBKey = servers().at(serverIndex).armoryDBKey;
   }
//   if (settings.runLocally) {
//      settings.armoryDBIp = QStringLiteral("127.0.0.1");
//      settings.armoryDBPort = appSettings_->GetDefaultArmoryLocalPort(appSettings_->get<NetworkType>(ApplicationSettings::netType));
//   }
   settings.socketType = appSettings_->GetArmorySocketType();

   settings.armoryExecutablePath = QDir::cleanPath(appSettings_->get<QString>(ApplicationSettings::armoryPathName));
   settings.dbDir = appSettings_->GetDBDir();
   settings.bitcoinBlocksDir = appSettings_->GetBitcoinBlocksDir();
   settings.dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

   return settings;
}

int ArmoryServersProvider::indexOfCurrent() const
{
   return indexOf(static_cast<ArmoryServer>(getArmorySettings()));
}

int ArmoryServersProvider::indexOfConnected() const
{
   return indexOf(static_cast<ArmoryServer>(connectedArmorySettings_));
}

int ArmoryServersProvider::indexOf(const QString &name) const
{
   // naive implementation
   QList<ArmoryServer> s = servers();
   for (int i = 0; i < s.size(); ++i) {
      if (s.at(i).name == name) {
         return i;
      }
   }
   return -1;
}

int ArmoryServersProvider::indexOf(const ArmoryServer &server) const
{
   return servers().indexOf(server);
}

int ArmoryServersProvider::indexOfIpPort(const std::string &srvIPPort) const
{
   QString ipPort = QString::fromStdString(srvIPPort);
   QStringList ipPortList = ipPort.split(QStringLiteral(":"));
   if (ipPortList.size() != 2) {
      return -1;
   }

   for (int i = 0; i < servers().size(); ++i) {
      if (servers().at(i).armoryDBIp == ipPortList.at(0) && servers().at(i).armoryDBPort == ipPortList.at(1).toInt()) {
         return i;
      }
   }
   return -1;
}

bool ArmoryServersProvider::add(const ArmoryServer &server)
{
   if (server.armoryDBPort < 1 || server.armoryDBPort > USHRT_MAX) {
      return false;
   }
   if (server.name.isEmpty()) {
      return false;
   }

   QList<ArmoryServer> serversData = servers();
   // check if server with already exist
   for (const ArmoryServer &s : serversData) {
      if (s.name == server.name) {
         return false;
      }
      if (s.armoryDBIp == server.armoryDBIp
          && s.armoryDBPort == server.armoryDBPort
          && s.netType == server.netType) {
         return false;
      }
   }

   QStringList serversTxt = appSettings_->get<QStringList>(ApplicationSettings::armoryServers);

   serversTxt.append(server.toTextSettings());
   appSettings_->set(ApplicationSettings::armoryServers, serversTxt);
   emit dataChanged();
   return true;
}

bool ArmoryServersProvider::replace(int index, const ArmoryServer &server)
{
   if (server.armoryDBPort < 1 || server.armoryDBPort > USHRT_MAX) {
      return false;
   }
   if (server.name.isEmpty()) {
      return false;
   }
   if (index < kDefaultServersCount) {
      return false;
   }

   QList<ArmoryServer> serversData = servers();
   if (index >= serversData.size()) {
      return false;
   }

   // check if server with already exist
   for (int i = 0; i < serversData.size(); ++i) {
      if (i == index) continue;

      const ArmoryServer &s = serversData.at(i);
      if (s.name == server.name) {
         return false;
      }
      if (s.armoryDBIp == server.armoryDBIp
          && s.armoryDBPort == server.armoryDBPort
          && s.netType == server.netType) {
         return false;
      }
   }

   QStringList serversTxt = appSettings_->get<QStringList>(ApplicationSettings::armoryServers);
   if (index - kDefaultServersCount >= serversTxt.size()) {
      return false;
   }

   serversTxt.replace(index - kDefaultServersCount, server.toTextSettings());
   appSettings_->set(ApplicationSettings::armoryServers, serversTxt);

   emit dataChanged();
   return true;
}

bool ArmoryServersProvider::remove(int index)
{
   if (index < kDefaultServersCount) {
      return false;
   }

   QStringList servers = appSettings_->get<QStringList>(ApplicationSettings::armoryServers);
   int indexToRemove = index - kDefaultServersCount;
   if (indexToRemove >= 0 && indexToRemove < servers.size()){
      servers.removeAt(indexToRemove);
      appSettings_->set(ApplicationSettings::armoryServers, servers);
      emit dataChanged();
      return true;
   }
   else {
      return false;
   }
}

void ArmoryServersProvider::setupServer(int index, bool needUpdate)
{
   QList<ArmoryServer> srvList = servers();
   if (index >= 0 && index < srvList.size()) {
      ArmoryServer server = srvList.at(index);
      appSettings_->set(ApplicationSettings::armoryDbName, server.name);
      appSettings_->set(ApplicationSettings::armoryDbIp, server.armoryDBIp);
      appSettings_->set(ApplicationSettings::armoryDbPort, server.armoryDBPort);
      appSettings_->set(ApplicationSettings::netType, static_cast<int>(server.netType));
      appSettings_->set(ApplicationSettings::runArmoryLocally, server.runLocally);

      if (needUpdate)
         emit dataChanged();
   }
}

void ArmoryServersProvider::addKey(const QString &address, int port, const QString &key)
{
   // find server
   int index = -1;
   for (int i = 0; i < servers().size(); ++i) {
      if (servers().at(i).armoryDBIp == address && servers().at(i).armoryDBPort == port) {
         index = i;
         break;
      }
   }

   if (index == -1){
      return;
   }

   if (index < ArmoryServersProvider::kDefaultServersCount) {
      QStringList defaultKeys = appSettings_->get<QStringList>(ApplicationSettings::defaultArmoryServersKeys);

      // defaultKeys might be empty
      while (defaultKeys.size() <= index) {
         defaultKeys.append(QString());
      }

      defaultKeys[index] = key;
      appSettings_->set(ApplicationSettings::defaultArmoryServersKeys, defaultKeys);
   }
   else {
      QStringList servers = appSettings_->get<QStringList>(ApplicationSettings::armoryServers);
      QString serverTxt = servers.at(index - ArmoryServersProvider::kDefaultServersCount);
      ArmoryServer server = ArmoryServer::fromTextSettings(serverTxt);
      server.armoryDBKey = key;
      servers[index - ArmoryServersProvider::kDefaultServersCount] = server.toTextSettings();

      appSettings_->set(ApplicationSettings::armoryServers, servers);
   }

   // update key for current server
   connectedArmorySettings_.armoryDBKey = key;

   emit dataChanged();
}

void ArmoryServersProvider::addKey(const std::string &srvIPPort, const BinaryData &srvPubKey)
{
   QString ipPort = QString::fromStdString(srvIPPort);
   QStringList ipPortList = ipPort.split(QStringLiteral(":"));
   if (ipPortList.size() == 2) {
      addKey(ipPortList.at(0)
             , ipPortList.at(1).toInt()
             , QString::fromStdString(srvPubKey.toHexStr()));
   }
}

ArmorySettings ArmoryServersProvider::connectedArmorySettings() const
{
   return connectedArmorySettings_;
}

void ArmoryServersProvider::setConnectedArmorySettings(const ArmorySettings &currentArmorySettings)
{
   connectedArmorySettings_ = currentArmorySettings;
}
